all: ass1

ass1:
	gcc -lnsl ass1.c -o ass1

clean:
	rm -f ass1
