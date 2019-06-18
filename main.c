#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
//#include "readURL.h"
#include "queue.h"

char* baseURL="https://en.wikipedia.org";

struct Page {
	char* url;
	char* name;
	long* links;
	int num_links;
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

long searchTable(char* URL, struct Page** page_hash_table, long int table_size){
	long index=hash(URL, table_size);
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
		index=(2*index)%table_size;
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

char* urlToName(char* url){
	int index=5;
	char* res=malloc(strlen(url)-5);
	while (url[index]!='\0'){
		index++;
		if (url[index]!='_'){
			res[index-6]=url[index];
		}
		else{
			res[index-6]=' ';
		}
	}
	return res;
}

void free_page(struct Page* p){
	if (p->num_links>0){
		free(p->links);
	}
	free(p->name);
	free(p->url);
	free(p);
}


char* readn(FILE* file, int n){
	char* s = malloc(n+1); s[n]='\0';
	fread(s, sizeof(char), n, file);
	return s;
}

void update_table(long base_loc, int num_links, char** linked_pages, struct Page** page_hash_table, struct Queue* q){
	long* link_locs=malloc(num_links*sizeof(long));
	long linked_loc;
	for (int i=0;i<num_links;i++){
		linked_loc=searchTable(linked_pages[i],page_hash_table, page_hash_table_size);
		if (linked_loc==-1){
			struct Page* p=malloc(sizeof(struct Page)); p->url=linked_pages[i]; p->num_links=0; p->links=NULL; p->name=urlToName(linked_pages[i]); //Make a page pointer
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



struct Page** load_page_hash_table(long int* table_size, char* table_path){
	FILE* read_file=fopen(table_path, "rb");
	fread(table_size, sizeof(table_size[0]), 1, read_file);
	fgetc(read_file);
	printf("\nTable size: %li\n", *table_size);
	struct Page* current_page;

	int str_len;


	struct Page** table = malloc(sizeof(struct Page*) * (*table_size));
	for (int i=0;i<(*table_size);i++){
		fread(&str_len, sizeof(str_len), 1, read_file);
		if (str_len>0){
			current_page=malloc(sizeof(struct Page));
			current_page->url=readn(read_file, str_len);
			fread(&str_len, sizeof(str_len), 1, read_file);
			current_page->name=readn(read_file, str_len);

			fread(&current_page->num_links, sizeof(int), 1, read_file);
			//printf("%i\n", current_page->num_links);
			if (current_page->num_links>0){
				current_page->links=malloc(sizeof(long int) * current_page->num_links);
				fread(current_page->links, sizeof(long int), current_page->num_links, read_file);
			}
			else{
				current_page->links=NULL;
			}
			table[i]=current_page;
		}
		else{
			table[i]=NULL;
		}
		fgetc(read_file);
	}
	printf("Table read finished.\n");
	return table;
}





// struct Child_info {
// 	pid_t PID;
// 	int pipe;
// 	long loc;
// };

// void intHandler(int dummy) {//Set up code to catch ctrl-C and killall
//     kill(-getpid(), SIGQUIT);
// }


void init(){
	//signal(SIGINT, intHandler);
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
		struct Page* first_page=malloc(sizeof(struct Page)); first_page->url="/wiki/C_(programming_language)"; first_page->num_links=0; first_page->links=NULL; first_page->name=urlToName("/wiki/C_(programming_language)");
		long first_index=addToTable(first_page,page_hash_table);
		reading_queue = createQueue();
		enqueue(reading_queue, first_index);
		//struct QNode* current_node;
		char* current_url;
		long current_loc;
		char** links_read; int num_links_read;
		long page_count=0;
		long processes_spun=0;

		

		int processed_queue_length = 192; //Must be substantially less than 1024, as we cannot have that many file handles open
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

		int scanning_index=0;

		//struct pollfd* poll_struct; poll_struct->events=POLLIN;
		while ((reading_queue->front!=NULL || queueLength(processes_to_create)!=processed_queue_length) && page_hash_table_spots_filled<page_hash_table_size*.99){// && page_hash_table_spots_filled<100000){ //Grab the next thing to process. If it's not NULL...
			while (processes_to_create->front!=NULL && reading_queue->front!=NULL){// && page_hash_table_spots_filled<100000){
				current_loc=dequeue(reading_queue); //Grab the next url to process
				current_url=page_hash_table[current_loc]->url;

				current_process_index=dequeue(processes_to_create);//Grab the index that we will put process info into

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


			while (true){ //Cycle until some pipe has data
				if (poll(&(struct pollfd){ .fd = child_pipes[scanning_index], .events = POLLIN }, 1, 0)==1) {
					current_process_index=scanning_index;
					scanning_index++;
					break;
				}
				//printf("Cycle!\n");
				scanning_index++; scanning_index%=processed_queue_length;
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
				fprintf(stderr, "Total read: %li. Block time: %f seconds. %i active blocks. %li free processes.\n", page_count, ((double)clock()-block_clock)/CLOCKS_PER_SEC, active_blocks, queueLength(processes_to_create));
				block_clock=clock();
			}
			if (page_count==10000){
				break;
			}
		}
		free(child_PIDs); free(child_pipes); free(child_locs);
		printf("Runtime: %f\n",((float)(clock()-t))/CLOCKS_PER_SEC);

		FILE* write_file=fopen(write_file_name,"wb");
		fwrite(&page_hash_table_size, sizeof(long int), 1, write_file);//Write the table size to the file
		fputc('\n', write_file);
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

				printf("%s\n", page_hash_table[i]->name);

				fwrite((const void *)&(page_hash_table[i]->num_links), sizeof(int), 1, write_file);
				fwrite((const void *)(page_hash_table[i]->links), sizeof(long int), page_hash_table[i]->num_links, write_file);
			}
			else{
				len=0;
				fwrite((const void *)&len, sizeof(int), 1, write_file);
			}
			fputc('\n', write_file);//Terminate every line of the file with a newline character
		}
		fclose(write_file);
		fflush(stdout);
	}
	if (strcmp(argv[1],"-path")==0){
		if (argc<4){
			fprintf(stderr, "Could not process command. Usage: wikigraph -path \"firstlink\" \"secondlink\" (-table \"tablepath\")");
		}
		char* table_path;
		if (argc==4){
			table_path="./table";
		}
		else{
			table_path=argv[5];
		}
		long int page_hash_table_size; struct Page** page_hash_table=load_page_hash_table(&page_hash_table_size, table_path);
		long int* predecessors = malloc(page_hash_table_size*sizeof(long int)); memset(predecessors, 0, page_hash_table_size*sizeof(long int));

		char* first_link; char* second_link;
		for (int i=0;i<strlen(argv[2]);i++){
			if (strncmp(&(argv[2][i]), "/wiki/", 6)==0){
				first_link=&(argv[2][i]);
			}
		}
		for (int i=0;i<strlen(argv[3]);i++){
			if (strncmp(&(argv[3][i]), "/wiki/", 6)==0){
				second_link=&(argv[3][i]);
			}
		}


		long int first_loc = searchTable(first_link, page_hash_table, page_hash_table_size); long int second_loc = searchTable(second_link, page_hash_table, page_hash_table_size);
		if (first_loc==-1){
			fprintf(stderr, "%s not found!\n", argv[2]);
			exit(-1);
		}
		if (second_loc==-1){
			fprintf(stderr, "%s not found!\n", argv[3]);
			exit(-1);
		}

		struct Queue* process_queue=createQueue();
		enqueue(process_queue, first_loc);
		long int next_loc;
		bool path_found = false;
		while (process_queue->front != NULL && !path_found){ //We queue pages to search on a first-seen, first-search basis. This is breadth-first.
			next_loc=dequeue(process_queue);
			for (int i=0; i<page_hash_table[next_loc]->num_links;i++){//For the given page, look at all of its children. If any have not been found, set their predecessors to the given page and queue them for search.
				if (predecessors[page_hash_table[next_loc]->links[i]] == 0){
					predecessors[page_hash_table[next_loc]->links[i]] = next_loc;
					enqueue(process_queue, page_hash_table[next_loc]->links[i]);
					if (page_hash_table[next_loc]->links[i] == second_loc){
						path_found=true;
						break;
					}
				}
			}
		}
		if (!path_found){
			printf("No path found!");
			exit(0);
		}
		long int path_length=1;
		long int trav_loc=second_loc;
		while (trav_loc!=first_loc){
			trav_loc=predecessors[trav_loc];
			path_length++;
		}
		char** names=malloc(sizeof(char*)*path_length);
		trav_loc=second_loc;
		long int trav_dist=0;
		while (trav_dist<path_length){
			names[path_length-1-trav_dist]=page_hash_table[trav_loc]->name;
			trav_dist++;
			trav_loc=predecessors[trav_loc];
		}
		printf("Shortest path found! Path length: %li. Path:\n", path_length);
		for (long int i=0;i<path_length;i++){
			printf("%li: %s\n", i+1, names[i]);
		}
		free(names);
		for (int i=0;i<page_hash_table_size;i++){
			if (page_hash_table[i]!=NULL){
				free_page(page_hash_table[i]);
			}
		}
		free(page_hash_table);
		while (process_queue->front!=NULL){
			dequeue(process_queue);
		}
		free(predecessors);
		free(process_queue);
	}
}