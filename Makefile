all: test ls cp ln mkdir

mkdir: mkdir.c ext2.h utils.h utils.o
	gcc -Wall -g -o ext2_mkdir mkdir.c ext2.h utils.h utils.o

#i called import.c because you're importing a file into the disk image
cp:	import.c ext2.h utils.h utils.o
	gcc -Wall -g -o ext2_cp import.c ext2.h utils.h utils.o

ls:	ls.c ext2.h utils.h utils.o
	gcc -Wall -g -o ext2_ls ls.c ext2.h utils.h utils.o

ln: ln.c ext2.h utils.h utils.o
	gcc -Wall -g -o ext2_ln ln.c ext2.h utils.h utils.o

test :  readimage.c disk.h ext2.h utils.o
	gcc -Wall -g -o readimage readimage.c ext2.h disk.h utils.o

utils: utils.c utils.h ext2.h disk.h
	gcc -Wall -g utils.c utils.h ext2.h readimage.h

clean : 
	rm -f *.o readimage ext2_* *~
