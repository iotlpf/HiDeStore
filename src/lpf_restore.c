/*
 * create by Pengfei Li
 *		2019.10.31
 */

#include "destor.h"
#include "jcr.h"
#include "recipe/recipestore.h"
#include "storage/containerstore.h"
#include "utils/lru_cache.h"
#include "utils/sync_queue.h"
#include "restore.h"
#include "lpf/active.h"


struct lruCache *ar_cache;

static pthread_t active_recipe_t;
static pthread_t active_read_t;
static pthread_t active_write_t;


static void* write_active_restore_data(void* arg) {

	char *p, *q;
	q = active_jcr.path + 1;/* ignore the first char*/
	/*
	 * recursively make directory
	 */
	while ((p = strchr(q, '/'))) {
		if (*p == *(p - 1)) {
			q++;
			continue;
		}
		*p = 0;
		if (access(active_jcr.path, 0) != 0) {
			mkdir(active_jcr.path, S_IRWXU | S_IRWXG | S_IRWXO);
		}
		*p = '/';
		q = p + 1;
	}

	struct chunk *c = NULL;
	FILE *fp = NULL;

	while ((c = sync_queue_pop(restore_chunk_queue))) {

		TIMER_DECLARE(1);
		TIMER_BEGIN(1);

		if (CHECK_CHUNK(c, CHUNK_FILE_START)) {
			VERBOSE("Restoring: %s", c->data);

			sds filepath = sdsdup(active_jcr.path);
			filepath = sdscat(filepath, c->data);

			int len = sdslen(active_jcr.path);
			char *q = filepath + len;
			char *p;
			while ((p = strchr(q, '/'))) {
				if (*p == *(p - 1)) {
					q++;
					continue;
				}
				*p = 0;
				if (access(filepath, 0) != 0) {
					mkdir(filepath, S_IRWXU | S_IRWXG | S_IRWXO);
				}
				*p = '/';
				q = p + 1;
			}

			if (destor.simulation_level == SIMULATION_NO) {
				assert(fp == NULL);
				fp = fopen(filepath, "w");
			}

			sdsfree(filepath);

		} else if (CHECK_CHUNK(c, CHUNK_FILE_END)) {
		    active_jcr.file_num++;

			if (fp)
				fclose(fp);
			fp = NULL;
		} else {
			assert(destor.simulation_level == SIMULATION_NO);
			VERBOSE("Restoring %d bytes", c->size);
			fwrite(c->data, c->size, 1, fp);
		}

		free_chunk(c);

		TIMER_END(1, active_jcr.write_chunk_time);
	}

    active_jcr.status = JCR_STATUS_DONE;
    return NULL;
}


static void* read_active_chunk_thread(void *arg){
	struct chunk* c;
	while ((c = sync_queue_pop(restore_recipe_queue))) {

		if (CHECK_CHUNK(c, CHUNK_FILE_START) || CHECK_CHUNK(c, CHUNK_FILE_END)) {
			sync_queue_push(restore_chunk_queue, c);
			continue;
		}

		TIMER_DECLARE(1);
		TIMER_BEGIN(1);

		struct cache_entry* ce = active_area_lookup(ac_table, &c->fp);
		if(ce) {
			struct chunk* ck = read_active_chunk(ce);
			sync_queue_push(restore_chunk_queue, ck);
		} else {
			struct container *con = lru_cache_lookup(ar_cache, &c->fp);
			if(con) {
				struct chunk *ck = get_chunk_in_container(con, &c->fp);
				sync_queue_push(restore_chunk_queue, ck);
			} else {
				WARNING("Error: can't find the chunk");
			}
		}

		TIMER_END(1, active_jcr.read_chunk_time);

		active_jcr.data_size += c->size;
		active_jcr.chunk_num++;
		free_chunk(c);
	}

	sync_queue_term(restore_chunk_queue);

	free_lru_cache(ar_cache);
	return NULL;
}

extern struct container* retrieve_archive_container_by_id(containerid id);

static void* active_lru_restore_thread(void *arg) {
	struct lruCache *cache;
	cache = new_lru_cache(destor.restore_cache[1], free_container,
				lookup_fingerprint_in_container);

	struct lruCache *active_cache;
	active_cache = new_lru_cache(10, free_container,
				lookup_fingerprint_in_container);

	struct chunk* c;
	while ((c = sync_queue_pop(restore_recipe_queue))) {

		if (CHECK_CHUNK(c, CHUNK_FILE_START) || CHECK_CHUNK(c, CHUNK_FILE_END)) {
			sync_queue_push(restore_chunk_queue, c);
			continue;
		}

		TIMER_DECLARE(1);
		TIMER_BEGIN(1);

		struct chunk* ck;
		if(c->id <= 0) {
			struct cache_entry* ce = active_area_lookup(ac_table, &c->fp);
			assert(ce);
			//ck = read_active_chunk(ce);
			struct container *con = lru_cache_lookup(active_cache, &ce->fp);
			if (!con) {
				VERBOSE("Restore cache: active container %lld is missed", ce->id);
				con = retrieve_active_container_by_id(ce->id);
				lru_cache_insert(active_cache, con, NULL, NULL);
				active_jcr.read_container_num++;
			}
			ck = get_chunk_in_container(con, &ce->fp);
		} else{
			struct container *con = lru_cache_lookup(cache, &c->fp);
			if (!con) {
				VERBOSE("Restore cache: archive container %lld is missed", c->id);
				con = retrieve_archive_container_by_id(c->id);
				lru_cache_insert(cache, con, NULL, NULL);
				active_jcr.read_container_num++;
			}
			ck = get_chunk_in_container(con, &c->fp);
		}		
		assert(ck);
		TIMER_END(1, active_jcr.read_chunk_time);
		sync_queue_push(restore_chunk_queue, ck);

		active_jcr.data_size += c->size;
		active_jcr.chunk_num++;
		free_chunk(c);
	}

	sync_queue_term(restore_chunk_queue);
	free_lru_cache(cache);
	free_lru_cache(active_cache);

	return NULL;
}


static void* read_active_recipe_thread(void *arg) {

	sds recipepath = sdsdup(destor.working_directory);
	//recipepath = sdscat(recipepath, "/active/recipes/nbv");
	recipepath = sdscat(recipepath, "/active/recipes/newestbv");
	char s[20];
	sprintf(s, "%d", active_jcr.bv->bv_num);
	recipepath = sdscat(recipepath, s);
	recipepath = sdscat(recipepath, ".recipe");
	FILE *recipe_fp;
	if ((recipe_fp = fopen(recipepath, "r")) == 0) {
		recipepath = sdsdup(destor.working_directory);
		recipepath = sdscat(recipepath, "/active/recipes/nbv");
		char s[20];
		sprintf(s, "%d", active_jcr.bv->bv_num);
		recipepath = sdscat(recipepath, s);
		recipepath = sdscat(recipepath, ".recipe");

		if((recipe_fp = fopen(recipepath, "r")) == 0){
			recipepath = sdsdup(destor.working_directory);
			recipepath = sdscat(recipepath, "/active/recipes/bv");
			char s[20];
			sprintf(s, "%d", active_jcr.bv->bv_num);
			recipepath = sdscat(recipepath, s);
			recipepath = sdscat(recipepath, ".recipe");

			if((recipe_fp = fopen(recipepath, "r")) == 0){
				fprintf(stderr, "Can not open %s!\n", recipepath);
				exit(1);
			}
		}
	}

	int i, j, k;
	for (i = 0; i < active_jcr.bv->number_of_files; i++) {
		TIMER_DECLARE(1);
		TIMER_BEGIN(1);

		struct fileRecipeMeta *r = read_next_file_recipe_meta(active_jcr.bv);

		struct chunk *c = new_chunk(sdslen(r->filename) + 1);
		strcpy(c->data, r->filename);
		SET_CHUNK(c, CHUNK_FILE_START);

		TIMER_END(1, active_jcr.read_recipe_time);

		sync_queue_push(restore_recipe_queue, c);

		for (j = 0; j < r->chunknum; j++) {
			TIMER_DECLARE(1);
			TIMER_BEGIN(1);

			//struct chunkPointer* cp = read_next_n_chunk_pointers(active_jcr.bv, 1, &k);
			
			struct chunkPointer *cp = (struct chunkPointer *) malloc(sizeof(struct chunkPointer));
			fread(&(cp->fp), sizeof(fingerprint), 1, recipe_fp);
			fread(&(cp->id), sizeof(containerid), 1, recipe_fp);
			fread(&(cp->size), sizeof(int32_t), 1, recipe_fp);

			struct chunk* c = new_chunk(0);
			memcpy(&c->fp, &cp->fp, sizeof(fingerprint));
			c->size = cp->size;
			c->id = cp->id;

			TIMER_END(1, active_jcr.read_recipe_time);

			sync_queue_push(restore_recipe_queue, c);
			free(cp);
		}

		c = new_chunk(0);
		SET_CHUNK(c, CHUNK_FILE_END);
		sync_queue_push(restore_recipe_queue, c);

		free_file_recipe_meta(r);
	}

	sync_queue_term(restore_recipe_queue);
	fclose(recipe_fp);
	return NULL;
}

void start_write_restore_data_phase() {
	pthread_create(&active_write_t, NULL, write_active_restore_data, NULL);
}

void stop_write_restore_data_phase() {
	pthread_join(active_write_t, NULL);
	NOTICE("write_restore_data phase stops successfully!");
}

void start_active_read_chunk_phase() {
	restore_chunk_queue = sync_queue_new(100);
	//if(pthread_create(&active_read_t, NULL, read_active_chunk_thread, NULL)!=0) {
	if(pthread_create(&active_read_t, NULL, active_lru_restore_thread, NULL)!=0) {
		WARNING("Error: active_read_t filed");
		exit(0);
	}
}

void stop_active_read_chunk_phase() {
	pthread_join(active_read_t, NULL);
	NOTICE("read_chunk phase stops successfully!");
}

void start_read_recipe_phase() {
	restore_recipe_queue = sync_queue_new(100);
	if(pthread_create(&active_recipe_t, NULL, read_active_recipe_thread, NULL)!=0) {
		WARNING("Error: active_recipe_t filed");
		exit(0);
	}
}

void stop_read_recipe_phase() {
	pthread_join(active_recipe_t, NULL);
	NOTICE("read_recipe phase stops successfully!");
}


static void* lru_restore_thread(void *arg) {
	struct lruCache *cache;
	if (destor.simulation_level >= SIMULATION_RESTORE)
		cache = new_lru_cache(destor.restore_cache[1], free_container_meta,
				lookup_fingerprint_in_container_meta);
	else
		cache = new_lru_cache(destor.restore_cache[1], free_container,
				lookup_fingerprint_in_container);

	struct chunk* c;
	while ((c = sync_queue_pop(restore_recipe_queue))) {

		if (CHECK_CHUNK(c, CHUNK_FILE_START) || CHECK_CHUNK(c, CHUNK_FILE_END)) {
			sync_queue_push(restore_chunk_queue, c);
			continue;
		}

		TIMER_DECLARE(1);
		TIMER_BEGIN(1);

		if (destor.simulation_level >= SIMULATION_RESTORE) {
			struct containerMeta *cm = lru_cache_lookup(cache, &c->fp);
			if (!cm) {
				VERBOSE("Restore cache: container %lld is missed", c->id);
				cm = retrieve_container_meta_by_id(c->id);
				assert(lookup_fingerprint_in_container_meta(cm, &c->fp));
				lru_cache_insert(cache, cm, NULL, NULL);
				jcr.read_container_num++;
			}

			TIMER_END(1, jcr.read_chunk_time);
		} else {
			struct container *con = lru_cache_lookup(cache, &c->fp);
			if (!con) {
				VERBOSE("Restore cache: container %lld is missed", c->id);
				con = retrieve_container_by_id(c->id);
				lru_cache_insert(cache, con, NULL, NULL);
				jcr.read_container_num++;
			}
			struct chunk *rc = get_chunk_in_container(con, &c->fp);
			assert(rc);
			TIMER_END(1, jcr.read_chunk_time);
			sync_queue_push(restore_chunk_queue, rc);
		}

		jcr.data_size += c->size;
		jcr.chunk_num++;
		free_chunk(c);
	}

	sync_queue_term(restore_chunk_queue);

	free_lru_cache(cache);

	return NULL;
}

static void* read_recipe_thread(void *arg) {

	int i, j, k;
	for (i = 0; i < jcr.bv->number_of_files; i++) {
		TIMER_DECLARE(1);
		TIMER_BEGIN(1);

		struct fileRecipeMeta *r = read_next_file_recipe_meta(jcr.bv);

		struct chunk *c = new_chunk(sdslen(r->filename) + 1);
		strcpy(c->data, r->filename);
		SET_CHUNK(c, CHUNK_FILE_START);

		TIMER_END(1, jcr.read_recipe_time);

		sync_queue_push(restore_recipe_queue, c);

		for (j = 0; j < r->chunknum; j++) {
			TIMER_DECLARE(1);
			TIMER_BEGIN(1);

			struct chunkPointer* cp = read_next_n_chunk_pointers(jcr.bv, 1, &k);

			struct chunk* c = new_chunk(0);
			memcpy(&c->fp, &cp->fp, sizeof(fingerprint));
			c->size = cp->size;
			c->id = cp->id;

			TIMER_END(1, jcr.read_recipe_time);

			sync_queue_push(restore_recipe_queue, c);
			free(cp);
		}

		c = new_chunk(0);
		SET_CHUNK(c, CHUNK_FILE_END);
		sync_queue_push(restore_recipe_queue, c);

		free_file_recipe_meta(r);
	}

	sync_queue_term(restore_recipe_queue);
	return NULL;
}

static void* write_restore_data(void* arg) {

	char *p, *q;
	q = jcr.path + 1;/* ignore the first char*/
	/*
	 * recursively make directory
	 */
	while ((p = strchr(q, '/'))) {
		if (*p == *(p - 1)) {
			q++;
			continue;
		}
		*p = 0;
		if (access(jcr.path, 0) != 0) {
			mkdir(jcr.path, S_IRWXU | S_IRWXG | S_IRWXO);
		}
		*p = '/';
		q = p + 1;
	}

	struct chunk *c = NULL;
	FILE *fp = NULL;

	while ((c = sync_queue_pop(restore_chunk_queue))) {

		TIMER_DECLARE(1);
		TIMER_BEGIN(1);

		if (CHECK_CHUNK(c, CHUNK_FILE_START)) {
			VERBOSE("Restoring: %s", c->data);

			sds filepath = sdsdup(jcr.path);
			filepath = sdscat(filepath, c->data);

			int len = sdslen(jcr.path);
			char *q = filepath + len;
			char *p;
			while ((p = strchr(q, '/'))) {
				if (*p == *(p - 1)) {
					q++;
					continue;
				}
				*p = 0;
				if (access(filepath, 0) != 0) {
					mkdir(filepath, S_IRWXU | S_IRWXG | S_IRWXO);
				}
				*p = '/';
				q = p + 1;
			}

			if (destor.simulation_level == SIMULATION_NO) {
				assert(fp == NULL);
				fp = fopen(filepath, "w");
			}

			sdsfree(filepath);

		} else if (CHECK_CHUNK(c, CHUNK_FILE_END)) {
		    jcr.file_num++;

			if (fp)
				fclose(fp);
			fp = NULL;
		} else {
			assert(destor.simulation_level == SIMULATION_NO);
			VERBOSE("Restoring %d bytes", c->size);
			fwrite(c->data, c->size, 1, fp);
		}

		free_chunk(c);

		TIMER_END(1, jcr.write_chunk_time);
	}

    jcr.status = JCR_STATUS_DONE;
    return NULL;
}

void lpf_restore(int revision, char *path) {

	init_recipe_store();
	init_container_store();

	init_restore_jcr(revision, path);

	destor_log(DESTOR_NOTICE, "backup path: %s", jcr.bv->path);
	destor_log(DESTOR_NOTICE, "restore to: %s", jcr.path);

	restore_chunk_queue = sync_queue_new(100);
	restore_recipe_queue = sync_queue_new(100);

	TIMER_DECLARE(1);
	TIMER_BEGIN(1);

	puts("==== restore begin ====");

    jcr.status = JCR_STATUS_RUNNING;
	pthread_t recipe_t, read_t, write_t;
	pthread_create(&recipe_t, NULL, read_recipe_thread, NULL);

	if (destor.restore_cache[0] == RESTORE_CACHE_LRU) {
		destor_log(DESTOR_NOTICE, "restore cache is LRU");
		pthread_create(&read_t, NULL, lru_restore_thread, NULL);
	} else if (destor.restore_cache[0] == RESTORE_CACHE_OPT) {
		destor_log(DESTOR_NOTICE, "restore cache is OPT");
		pthread_create(&read_t, NULL, optimal_restore_thread, NULL);
	} else if (destor.restore_cache[0] == RESTORE_CACHE_ASM) {
		destor_log(DESTOR_NOTICE, "restore cache is ASM");
		pthread_create(&read_t, NULL, assembly_restore_thread, NULL);
	} else {
		fprintf(stderr, "Invalid restore cache.\n");
		exit(1);
	}

	pthread_create(&write_t, NULL, write_restore_data, NULL);

    do{
        sleep(5);
        /*time_t now = time(NULL);*/
        fprintf(stderr, "%" PRId64 " bytes, %" PRId32 " chunks, %d files processed\r", 
                jcr.data_size, jcr.chunk_num, jcr.file_num);
    }while(jcr.status == JCR_STATUS_RUNNING || jcr.status != JCR_STATUS_DONE);
    fprintf(stderr, "%" PRId64 " bytes, %" PRId32 " chunks, %d files processed\n", 
        jcr.data_size, jcr.chunk_num, jcr.file_num);

	assert(sync_queue_size(restore_chunk_queue) == 0);
	assert(sync_queue_size(restore_recipe_queue) == 0);

	free_backup_version(jcr.bv);

	TIMER_END(1, jcr.total_time);
	puts("==== restore end ====");

	printf("restore path: %s\n", jcr.path);
	printf("number of files: %" PRId32 "\n", jcr.file_num);
	printf("number of chunks: %" PRId32"\n", jcr.chunk_num);
	printf("total size(B): %" PRId64 "\n", jcr.data_size);
	printf("speed factor: %.2f\n",
			jcr.data_size / (1024.0 * 1024 * jcr.read_container_num));

	

	close_container_store();
	close_recipe_store();
}