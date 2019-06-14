wikigraph: main.o queue.o readURL.o
	gcc -o wikigraph -g3 -Wall -std=c99 -I/home/stephen/anaconda3/include -L/home/stephen/anaconda3/lib/ main.o queue.o readURL.o -l curl # -L/usr/local/lib -l pcre

main.o: main.c
	gcc -o main.o -g3 -Wall -std=c99 main.c -c -L/usr/local/lib #-l pcre

queue.o: queue.c
	gcc -o queue.o -g3 -Wall -std=c99 queue.c -c

readURL.o: readURL.c
	gcc -o readURL.o -g3 -Wall -std=c99 -I/home/stephen/anaconda3/include readURL.c -c -l curl 