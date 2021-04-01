make: readerwriter
	gcc -std=c99 main.c readerwriter.o  -pthread -o rwmain
readerwriter: readerwriter.c readerwriter.h
	gcc -std=c99 -c readerwriter.c -pthread

clean:
	rm -f *.o rwmain
