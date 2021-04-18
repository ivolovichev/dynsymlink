all: dlfs

dlfs: dlfs.c conf.c dlfs.h
	gcc ${CFLAGS} -O2 -o dlfs `pkg-config --cflags --libs fuse3 json-c` dlfs.c conf.c
