CC := gcc
CFLAGS := -g `pkg-config --cflags --libs glib-2.0`
LDFLAGS := -lpthread -lcrypto -lm


all: check

check: check.c
	$(CC) $(CFLAGS) check.c -o check $(LDFLAGS)