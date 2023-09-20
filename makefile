simpleshell: simpleshell.c
	gcc -o simpleshell simpleshell.c -Wall
clean:
	rm *.o simpleshell
