filesys: shell.o filesys.o
	gcc -o filesys shell.o filesys.o -lpthread

shell.o: shell.c
	gcc -c shell.c

filesys.o: filesys.c filesys.h
	gcc -c filesys.c

clean: 
	rm -f filesys
	rm -f *.o