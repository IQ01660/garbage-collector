CC            = gcc
#SPECIAL_FLAGS = -ggdb -Wall -DDEBUG_ALLOC
SPECIAL_FLAGS = -ggdb -Wall
#SPECIAL_FLAGS = -O3
CFLAGS        = -std=gnu99 $(SPECIAL_FLAGS)

gctest: gctest.c gc.h bf-gc.o safeio.o
	$(CC) $(CFLAGS) -o gctest gctest.c bf-gc.o safeio.o

bf-gc.o: gc.h bf-gc.c
	$(CC) $(CFLAGS) -c bf-gc.c

safeio.o: safeio.c safeio.h
	$(CC) $(CFLAGS) -c safeio.c

docs:
	doxygen

clean:
	rm -rf *.o gctest
