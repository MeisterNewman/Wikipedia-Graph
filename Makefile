# make all: 
# 	make grabLinks
# 	make wikigraph

# wikigraph: main.o queue.o
# 	make grabLinks
# 	gcc -o wikigraph -g3 -Wall -std=c99 -I/home/stephen/anaconda3/include -L/home/stephen/anaconda3/lib/ main.o queue.o -l curl # -L/usr/local/lib -l pcre

# grabLinks: grabLinks.o readURL.o
# 	gcc -o grabLinks -g3 -Wall -std=c99 -I/home/stephen/anaconda3/include -L/home/stephen/anaconda3/lib/ grabLinks.o readURL.o -l curl

# grabLinks.o: grabLinks.c
# 	gcc -o grabLinks.o -g3 -Wall -std=c99 grabLinks.c -c

# main.o: main.c
# 	gcc -o main.o -g3 -Wall -std=c99 main.c -c -L/usr/local/lib #-l pcre

# queue.o: queue.c
# 	gcc -o queue.o -g3 -Wall -std=c99 queue.c -c

# readURL.o: readURL.c
# 	gcc -o readURL.o -g3 -Wall -std=c99 -I/home/stephen/anaconda3/include readURL.c -c -l curl 

wikigraph: main.o queue.o
	gcc -o wikigraph -g3 -Wall main.o queue.o

main.o: main.c
	gcc -o main.o -g3 -Wall main.c -c 

queue.o: queue.c
	gcc -o queue.o -g3 -Wall queue.c -c