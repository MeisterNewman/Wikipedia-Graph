#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "readURL.h"

char* baseURL="https://en.wikipedia.org";

bool trim_pound(char** a){ //Trims the pound sign away from a string
	long int index=strchr((*a),'#')-(*a);
	if (index>=0){
		(*a)=realloc((*a),(index+1)*sizeof(char));
		(*a)[index]='\0';
		return true;
	}
	return false;
}

int strOrd(const void* s, const void* t){//Recursively compare two strings
	const char* a = (const char*)s; const char* b = (const char*)t;
	if (a[0]=='\0' || b[0]>a[0]){
		return -1;
	}
	if (b[0]=='\0' || a[0]<b[0]){
		return 1;
	}
	return strOrd(&a[1],&b[1]);
}

char** readPage(char* URL, int* num_links){
	int baseLen=strlen(baseURL);
	int tailLen=strlen(URL);
	char* fullURL=malloc(baseLen+tailLen+1);
	memcpy(fullURL,baseURL,baseLen); memcpy(&fullURL[baseLen],URL,tailLen); fullURL[baseLen+tailLen]='\0';
	char* pageHTML=getHTML(fullURL);
	free(fullURL);

	int num_matches;
	char** links=scanLinks(pageHTML,&num_matches);
	char* match;

	int num_success=0;
	char** linked_pages=malloc(num_matches*sizeof(char*));

	qsort(links, num_matches, sizeof(links), &strOrd);

	bool ignore;
	for (int i=0;i<num_matches;i++){
		//printf("Link being checked: %i\n",i);
		ignore=false;
		match = links[i];
		trim_pound(&match);
		if (strcmp(URL,match)==0){ //If this link links to its own page, ignore it
			ignore=true;
		}
		if (strchr(match,':')!=0){//If it is a link to a special wikipedia page type (for instance, a 'Category'), ignore it
			ignore=true;
		}
		if (i>0){
			if (strcmp(links[i-1],match)==0){
				ignore=true;
			}
		}
		if (ignore){
			free(match);
		}
		else{
			linked_pages[num_success]=match;
			num_success++;
		}
	}
	free(links);//All individual links have been freed or are being carried on.
	free(pageHTML);
	//printf("Num distinct links found: %i\n", num_success);
	linked_pages=realloc(linked_pages,num_success*sizeof(char*)); //Scale this down to just hold the new_page links
	*num_links=num_success;
	printf("Num successes: %i\n", num_success);
	return linked_pages;
}

int main(int argc, char* argv[]){
	//fprintf(stderr, "Starting downlink on %s!\n", argv[1]);
	int num_links_read; int link_length;
	char** links_read = readPage(argv[1], &num_links_read); //Process the next queued page.
	write(STDOUT_FILENO, &num_links_read, sizeof(num_links_read)); //Write number of strings to the pipe
	for (int i=0;i<num_links_read;i++){ //For each string
		link_length=strlen(links_read[i]);
		write(STDOUT_FILENO, &link_length, sizeof(link_length)); //Write its length
		write(STDOUT_FILENO,links_read[i],link_length+1); //Write the string, including null terminator
		free(links_read[i]);
	}
	close(STDOUT_FILENO);
	//fprintf(stderr, "Returned %i links!\n", num_links_read);
	//fprintf(stderr, "Time taken to grab link: %f ms\n", ((float)(clock()-t))/CLOCKS_PER_SEC*1000);
	//return(atoi(argv[2]));
	exit(0);

}