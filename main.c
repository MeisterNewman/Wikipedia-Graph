#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
//#include "readURL.h"
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
long page_hash_table_spots_filled=0;

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
	//long int steps=0;
	while (true){
		if (page_hash_table[index]==NULL){
			//fprintf(stderr, "Num steps: %li\n", steps);
			return -1;
		}
		if (strcmp((page_hash_table[index])->url,URL)==0){
			//fprintf(stderr, "Num steps: %li\n", steps);
			return index;
		}
		index=(2*index)%page_hash_table_size;
		//steps++;
	}
}

long addToTable(struct Page* page, struct Page** page_hash_table){
	long index=hash(page->url, page_hash_table_size);
	while (true){
		if (page_hash_table[index]==NULL){
			page_hash_table[index]=page;
			return index;
		}
		index=(2*index)%page_hash_table_size;
	}
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
			page_hash_table_spots_filled++;
		}
		else{
			free(linked_pages[i]); //It's not getting reused as the url of a made page, so we must free here.
		}
		link_locs[i]=linked_loc;
	}
	page_hash_table[base_loc]->links=link_locs; page_hash_table[base_loc]->num_links=num_links; //Add the links to the table entry.
	free(linked_pages);
}





// struct Child_info {
// 	pid_t PID;
// 	int pipe;
// 	long loc;
// };



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

		struct Queue* reading_queue;
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

		

		int processed_queue_length = 256;
		int current_process_index;


		pid_t* child_PIDs=malloc(sizeof(pid_t)*processed_queue_length); memset(child_PIDs,0,sizeof(pid_t)*processed_queue_length);
		int* child_pipes=malloc(sizeof(int)*processed_queue_length); memset(child_pipes,0,sizeof(int)*processed_queue_length);
		long* child_locs=malloc(sizeof(long)*processed_queue_length); memset(child_locs,0,sizeof(long)*processed_queue_length);
		//char** child_urls=malloc(sizeof(char*)*processed_queue_length); memset(child_PIDs,0,sizeof(char*)*processed_queue_length);

		struct Queue* processes_to_create = createQueue();
		for (int i=0;i<processed_queue_length;i++){
			enqueue(processes_to_create, i);
		}



		pid_t PID;
		int pipe_ends[2];

		int link_length;

		clock_t t=clock();
		clock_t block_clock=clock();

		//struct pollfd* poll_struct; poll_struct->events=POLLIN;
		while ((reading_queue->front!=NULL || queueLength(processes_to_create)!=processed_queue_length) && page_hash_table_spots_filled<page_hash_table_size*.99){// && page_hash_table_spots_filled<100000){ //Grab the next thing to process. If it's not NULL...
			
			// t=clock();
			//printf("\nNext key: %li\n", current_loc);
			
			//If we have space in the queue of things to be processed and space in the processing table, fork a process to read the next page in the queue
			
			while (processes_to_create->front!=NULL && reading_queue->front!=NULL){// && page_hash_table_spots_filled<100000){
				current_node=dequeue(reading_queue); //Grab the next url to process
				current_loc=current_node->key;
				current_url=page_hash_table[current_loc]->url;
				free(current_node);

				current_node=dequeue(processes_to_create);//Grab the index that we will put process info into
				current_process_index=current_node->key;
				free(current_node);

				pipe(pipe_ends);
				if ((PID=fork())<0){ //Fork. If error:
					fprintf(stderr, "Fork failed!\n");
					exit(-1);
				}
				else if (PID==0){ //If we are the child, process the page.
					close(pipe_ends[0]);//Close the read end of the pipe immediately.
					dup2(pipe_ends[1],STDOUT_FILENO); //Make this process write out to the pipe
					execlp("./grabLinks", "./grabLinks", current_url, (char*) NULL);//Turn to the grabLinks program 
					fprintf(stderr, "Failed to convert process!");
				}
				else{ //If we are the parent, then do the following
					close(pipe_ends[1]);
					child_pipes[current_process_index]=pipe_ends[0];
					child_PIDs[current_process_index]=PID;
					//child_urls[processed_queue_last_started]=current_url;
					child_locs[current_process_index]=current_loc;
				}
				processes_spun++;
			}
			// int ct=0;
			// while (true){ //Cycle until some pipe has data
			// 	poll_struct->fd=child_pipes[ct];
			// 	if (poll(poll_struct, 1, 0)==1){
			// 		current_process_index=ct;
			// 		break;
			// 	}
			// 	ct+=1;ct%=processed_queue_length;
			// 	if (ct==0){
			// 		current_process_index=-1;
			// 		processed_queue_length++;
			// 		child_PIDs=realloc(child_PIDs, sizeof(pid_t)*processed_queue_length);
			// 		child_pipes=realloc(child_pipes, sizeof(int)*processed_queue_length);
			// 		child_locs=realloc(child_locs, sizeof(long)*processed_queue_length);
			// 		enqueue(processes_to_create,processed_queue_length-1);
			// 		child_locs[processed_queue_length-1]=child_pipes[processed_queue_length-1]=child_PIDs[processed_queue_length-1]=0;
			// 	}
			// }
			current_process_index=0;
			while (true){ //Cycle until some pipe has data
				if (poll(&(struct pollfd){ .fd = child_pipes[current_process_index], .events = POLLIN }, 1, 0)==1) {
					break;
				}
				//printf("Cycle!\n");
				current_process_index++; current_process_index%=processed_queue_length;
			}
			read(child_pipes[current_process_index],&num_links_read,sizeof(num_links_read)); //Read off number of links
			links_read=malloc(num_links_read*sizeof(char*)); // Allocate the sapace for each link read out
			for (int i=0;i<num_links_read;i++){ //For each link
				read(child_pipes[current_process_index], &link_length, sizeof(link_length)); //Read length
				links_read[i]=malloc(link_length+1); //Allocate space to hold the link
				read(child_pipes[current_process_index],links_read[i],link_length+1); //Write the string, including null terminator
			}
			close(child_pipes[current_process_index]);
			update_table(child_locs[current_process_index], num_links_read, links_read, page_hash_table, reading_queue);
			enqueue(processes_to_create,current_process_index);
			page_count++; //Update page count
			waitpid(-1, NULL, WNOHANG);
			child_locs[current_process_index]=child_pipes[current_process_index]=child_PIDs[current_process_index]=0;

			if (page_count%1000==0){
				int active_blocks=processed_queue_length;
				for (int i=0;i<processed_queue_length;i++){
					if (child_PIDs[i]==0){
						active_blocks--;
					}
				}
				fprintf(stderr, "Total read: %li. Block time: %f seconds. %i active blocks. %i free processes.\n", page_count, ((double)clock()-block_clock)/CLOCKS_PER_SEC, active_blocks, queueLength(processes_to_create));
				block_clock=clock();
			}
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
	fflush(stdout);
	
}