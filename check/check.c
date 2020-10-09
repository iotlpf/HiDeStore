#include <openssl/sha.h>
#include <glib.h>
#include <stdio.h>

typedef unsigned char fingerprint[20];

int main()
{
	char data[256];
	for(int i=0; i<256; i++){
		data[i]='a';
	} 
	fingerprint fp;
	SHA_CTX ctx;
	SHA_Init(&ctx);
	SHA_Update(&ctx, data, 256);
	SHA_Final(fp, &ctx);
	printf("check: OK\n");
	return 0;
}