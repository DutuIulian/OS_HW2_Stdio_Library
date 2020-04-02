CC=gcc

all: libso_stdio.so

build: libso_stdio.so

libso_stdio.so: so_stdio.o
	$(CC) -shared -o libso_stdio.so so_stdio.o

so_stdio.o: so_stdio.c so_stdio.h
	$(CC) -c -o so_stdio.o so_stdio.c -fPIC -g3

.PHONY: clean
clean:
	rm libso_stdio.so *.o
