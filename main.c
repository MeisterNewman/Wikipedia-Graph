#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include "readURL.h"
#include "queue.h"


char* baseURL="https://en.wikipedia.org";

struct Page {
	char* url;
	char* name;
	long* links;
	int num_links;
	bool initialized;
};

long int page_hash_table_size = 8003537; //Bigger than the current size of Wikipedia. Also prime.
struct Page** page_hash_table;

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

void update_table(long base_loc, int num_links, char** linked_pages, struct Page** page_hash_table, struct Queue* q){
	long* link_locs=malloc(num_links*sizeof(long));
	long linked_loc;
	for (int i=0;i<num_links;i++){
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
	}
	page_hash_table[base_loc]->links=link_locs; page_hash_table[base_loc]->num_links=num_links; //Add the links to the table entry.
	free(linked_pages);
}



char** readPage(char* URL, struct Page** page_hash_table, int* num_links){
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
	free(pageHTML);
	//printf("Num distinct links found: %i\n", num_success);
	linked_pages=realloc(linked_pages,num_success*sizeof(char*)); //Scale this down to just hold the new_page links
	*num_links=num_success;
	return linked_pages;
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
		char* current_url;
		long current_loc;
		char** links_read; int num_links_read;
		long page_count=0;
		long processes_spun=0;
		clock_t t=clock();


		int processed_queue_length =64;
		int processed_queue_last_read = 0;
		int processed_queue_last_started = 0;

		pid_t* child_PIDs=malloc(sizeof(pid_t)*processed_queue_length); memset(child_PIDs,0,sizeof(pid_t)*processed_queue_length);
		int* child_pipes=malloc(sizeof(int)*processed_queue_length); memset(child_pipes,0,sizeof(int)*processed_queue_length);
		long* child_locs=malloc(sizeof(long)*processed_queue_length); memset(child_locs,0,sizeof(long)*processed_queue_length);
		//char** child_urls=malloc(sizeof(char*)*processed_queue_length); memset(child_PIDs,0,sizeof(char*)*processed_queue_length);



		pid_t PID;
		int pipe_ends[2];

		int link_length;


		while ((reading_queue->front!=NULL || processed_queue_last_started!=processed_queue_last_read) && page_count<page_hash_table_size*.99 && processes_spun<1030){ //Grab the next thing to process. If it's not NULL...
			
			//printf("\nNext key: %li\n", current_loc);
			
			//If we have space in the queue of things to be processed and space in the processing table, fork a process to read the next page in the queue
			while (processed_queue_last_started != processed_queue_last_read-1 && !(processed_queue_last_started==0 && processed_queue_last_read==processed_queue_length-1) && reading_queue->front!=NULL && processes_spun<1030){
				current_node=dequeue(reading_queue);
				current_loc=current_node->key;
				current_url=page_hash_table[current_loc]->url;
				free(current_node);

				pipe(pipe_ends);
				if ((PID=fork())<0){ //Fork. If error:
					fprintf(stderr, "Fork failed!\n");
					exit(-1);
				}
				else if (PID==0){ //If we are the child, process the page.
					readURLInit();
					close(pipe_ends[0]);//Close the read end of the pipe immediately.
					links_read = readPage(current_url, page_hash_table, &num_links_read); //Process the next queued page.
					write(pipe_ends[1], &num_links_read, sizeof(num_links_read)); //Write number of strings to the pipe
					for (int i=0;i<num_links_read;i++){ //For each string
						link_length=strlen(links_read[i]);
						write(pipe_ends[1], &link_length, sizeof(link_length)); //Write its length
						write(pipe_ends[1],links_read[i],link_length+1); //Write the string, including null terminator
						free(links_read[i]);
					}
					close(pipe_ends[1]); //Close the write end of the pipe.
					free(links_read);
					exit(0);
				}
				else{ //If we are the parent, then do the following
					processed_queue_last_started=(processed_queue_last_started+1)%processed_queue_length; //Increment to the next one
					close(pipe_ends[1]);
					child_pipes[processed_queue_last_started]=pipe_ends[0];
					child_PIDs[processed_queue_last_started]=PID;
					//child_urls[processed_queue_last_started]=current_url;
					child_locs[processed_queue_last_started]=current_loc;
				}
				processes_spun++;
			}

			//Pick one element out of the table and process it
			processed_queue_last_read=(processed_queue_last_read+1)%processed_queue_length; //Increment to the next one. This is the one we will read out.

			read(child_pipes[processed_queue_last_read],&num_links_read,sizeof(num_links_read)); //Read off number of links
			links_read=malloc(num_links_read*sizeof(char*)); // Allocate the sapace for each link read out

			for (int i=0;i<num_links_read;i++){ //For each link
				read(child_pipes[processed_queue_last_read], &link_length, sizeof(link_length)); //Read length
				links_read[i]=malloc(link_length+1); //Allocate space to hold the link
				read(child_pipes[processed_queue_last_read],links_read[i],link_length+1); //Write the string, including null terminator
			}
			update_table(child_locs[processed_queue_last_read], num_links_read, links_read, page_hash_table, reading_queue);
			waitpid(child_PIDs[processed_queue_last_read], NULL, 0);
			page_count++; //Update page count
		}
		free(child_PIDs); free(child_pipes); free(child_locs);
		printf("Runtime: %f\n",((float)(clock()-t))/CLOCKS_PER_SEC);

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