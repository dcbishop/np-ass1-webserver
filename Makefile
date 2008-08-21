all: ass1

ass1: ass1.c
	gcc -lnsl -lpthread ass1.c -o ass1

clean:
	rm -f ass1
