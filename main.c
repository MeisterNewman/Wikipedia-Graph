#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <string.h>
#include <time.h> 
//#include <curl/curl.h>
#define PCRE2_CODE_UNIT_WIDTH 8
//#include <pcre2.h>
#include "readURL.h"
#include "queue.h"


char* baseURL="https://en.wikipedia.org";
// const unsigned char* URL_regex_string=(const unsigned char*)"href=\"(\\/wiki\\/[^\"]*)\""; //Matches links. Note that this will include subsection links, including to the same page. All of these have a "#" character separating the page link and the subpage link, so that should be enough to catch them



struct Page {
	char* url;
	char* name;
	long* links;
	int num_links;
	bool initialized;
};

long int page_hash_table_size = 8003537; //Bigger than the current size of Wikipedia. Also prime.
struct Page** page_hash_table;

// void initPage(Page* page, char* url){
// 	page->str_len = 0;
// 	page->url = url;
// 	page->links = NULL;
// 	page->num_links = -1;
// }

// void Page__process(struct Page* self, int num_links, Page** links){
// 	int str_len;
// 	self->num_links = num_links;
// 	self->links=malloc(sizeof(int)*num_links);
// 	for (int i=0;i<num_links;i++){//Copy all strings into the object
// 		self->links[i]=links[i];
// 	}
// }



unsigned long hash(char* s, int modulus){ //djb2 by Dan Bernstein
	char* str = s;
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)){
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return (hash%modulus);
}

long searchTable(char* URL, struct Page** page_hash_table){
	long index=hash(URL, page_hash_table_size);
	while (true){
		if (page_hash_table[index]==NULL){
			return -1;
		}
		if (strcmp((page_hash_table[index])->url,URL)==0){
			return index;
		}
		index++;
		if (index==page_hash_table_size){
			index=0;
		}
	}
}

long addToTable(struct Page* page, struct Page** page_hash_table){
	long index=hash(page->url, page_hash_table_size);
	while (true){
		if (page_hash_table[index]==NULL){
			page_hash_table[index]=page;
			return index;
		}
		index++;
		if (index==page_hash_table_size){
			index=0;
		}
	}
}


bool trim_pound(char** a){ //Trims the pound sign away from a string
	long int index=strchr((*a),'#')-(*a);
	if (index>=0){
		(*a)=realloc((*a),(index+1)*sizeof(char));
		(*a)[index]='\0';
		return true;
	}
	return false;
}

void readPage(long loc, struct Page** page_hash_table, struct Queue *q){
	char* URL=page_hash_table[loc]->url;
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
		if (!ignore){
			for (int j=num_success-1;j>=0;j--){ // If we've already logged a link to this string, ignore it. Going backwards gives large speedup as links tend to be consecutive.
				if (strcmp(linked_pages[j],match)==0){
					ignore=true;
					j=-1;
				}
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
	printf("Num distinct links found: %i\n", num_success);
	linked_pages=realloc(linked_pages,num_success*sizeof(char*)); //Scale this down to just hold the new_page links
	long* link_locs=malloc(num_success*sizeof(long));

	long linked_loc;
	for (int i=0;i<num_success;i++){
		linked_loc=searchTable(linked_pages[i],page_hash_table);
		if (linked_loc==-1){
			struct Page* p=malloc(sizeof(struct Page)); p->url=linked_pages[i]; p->initialized=false; p->num_links=0; p->links=NULL; p->name=""; //Make a page pointer
			linked_loc=addToTable(p,page_hash_table);
			enqueue(q, linked_loc); //Add this page to the list of things to be covered.
		}
		else{
			free(linked_pages[i]); //It's not getting reused as the url of a made page, so we must free here.
		}
		link_locs[i]=linked_loc;
		//free(linked_pages[i]);
	}
	page_hash_table[loc]->links=link_locs; page_hash_table[loc]->num_links=num_success; //Add the links to the table entry.


	free(linked_pages); //No subpages actually need to be freed here because they all were when they were discarded as irrelevant or when their indices were found
	free(pageHTML);
	//We don't free link_locs, as it's passed on to the entry in the hash table.
}






struct Queue *reading_queue;




void init(){
	page_hash_table = malloc(sizeof(struct Page*) * page_hash_table_size);
	memset(page_hash_table, 0, sizeof(struct Page*) * page_hash_table_size);

	
}

int main(int argc, char* argv[]){
	init();
	if (argc==1){
		printf("No arguments passed!\n");
		exit(0);
	}
	if (strcmp(argv[1],"-build")==0){
		char* write_file_name;
		if (argc<3){
			printf("No storage filename selected. Defaulting to \"./table\"\n");
			write_file_name="./table";
		}
		else{
			write_file_name=argv[2];
		}
		struct Page* first_page=malloc(sizeof(struct Page)); first_page->url="/wiki/C_(programming_language)"; first_page->initialized=false; first_page->num_links=0; first_page->links=NULL; first_page->name="";
		long first_index=addToTable(first_page,page_hash_table);
		reading_queue = createQueue();
		enqueue(reading_queue, first_index);
		struct QNode* current_node;
		long current_loc;
		long page_count=0;
		clock_t t=clock();

		while ((current_node=dequeue(reading_queue))!=NULL && page_count<page_hash_table_size*.99){ //Grab the next thing to process. If it's not NULL...
			current_loc=current_node->key;
			printf("\nNext key: %li\n", current_loc);
			readPage(current_loc, page_hash_table, reading_queue); //Process the next queued page.
			free(current_node);
			page_count++;
		}
		printf("Runtime: %f\n",((float)(clock()-t))/CLOCKS_PER_SEC);
		free(current_node);

		FILE* write_file=fopen(write_file_name,"wb");
		fwrite(&page_hash_table_size, sizeof(long int), 1, write_file);//Write the table size to the file

		int len;
		for (int i=0;i<page_hash_table_size;i++){
			if (page_hash_table[i]!=NULL){
				//Write url length, url, name length, name, num_links, (link)*num_links. If url_length is 0, that spot is unfilled.
				len=strlen(page_hash_table[i]->url);
				fwrite((const void *)&len, sizeof(int), 1, write_file);
				fputs(page_hash_table[i]->url, write_file);

				len=strlen(page_hash_table[i]->name);
				fwrite((const void *)&len, sizeof(int), 1, write_file);
				fputs(page_hash_table[i]->name, write_file);

				fwrite((const void *)&(page_hash_table[i]->num_links), sizeof(int), 1, write_file);
				for (int j=0;j<page_hash_table[i]->num_links;j++){
					fwrite((const void *)&(page_hash_table[i]->links[j]), sizeof(long int), 1, write_file);
				}
			}
			else{
				len=0;
				fwrite((const void *)&len, sizeof(int), 1, write_file);
			}
			fputc('\n', write_file);//Terminate every line of the file with a newline character
		}
		fclose(write_file);
		}
	
}