/**
A Wikipedia mapping and analysis program by Stephen Newman. 2019. All software licensed under the GPL v3 license when applicable.
**/

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


#define BUF_LENGTH 10

struct CBuf { //A cyclic buffer to keep track of the last few characters
	char buffer[BUF_LENGTH];
	int current_index;
};

bool isPrime(long x){
	for (long i=2; i*i<x; i++){
		if (x%i==0){
			return false;
		}
	}
	return true;
}

void write_char(struct CBuf* Buf, char c){
	Buf->buffer[Buf->current_index]=c;
	Buf->current_index++;
	if (Buf->current_index==BUF_LENGTH){
		Buf->current_index=0;
	}
}

bool just_read(struct CBuf* Buf, char* s, int l){ //Given a string, check whether it was the most recent thing written to the buffer
	int buf_ind=Buf->current_index-l;
	if (buf_ind<0){
		buf_ind+=BUF_LENGTH;
	}
	for (int i=0;i<l;i++){
		if (s[i]!=Buf->buffer[buf_ind]){
			return false;
		}
		buf_ind++;
		if (buf_ind==BUF_LENGTH){
			buf_ind=0;
		}
	}
	return true;
}

char* read_until(FILE* f, char* s, int l){ //Reads from a file until a terminating string, then outputs the read text up to that point (but not including the terminator)
	long max_len=128;
	long current_len=0;
	char* o = malloc(max_len);
	struct CBuf* buf = malloc(sizeof(struct CBuf)); memset(buf->buffer,0,10); buf->current_index=0;
	while (1){
		o[current_len]=fgetc(f);
		write_char(buf, o[current_len]);
		current_len++;
		if (just_read(buf, s, l) || o[current_len-1]==EOF){
			o=realloc(o, current_len-strlen(s)+1);
			o[current_len-strlen(s)]='\0';
			free(buf->buffer);
			return o;
		}
		if (current_len==max_len){
			max_len+=128;
			o=realloc(o,max_len);
		}
	}
}




struct Page {
	char* name;
	long* links;
	int num_links;
	bool is_redirect;
};


bool caseless_equal(char* a, char* b){
	int index=0;
	while (1){
		if (a[index]==b[index]){
			if (a[index]=='\0') return true;
			index++;
		}
		else if (index==0 && ((a[index]>=65 && a[index]<=90 && b[index]==a[index]+32) || (b[index]>=65 && b[index]<=90 && a[index]==b[index]+32))){
			index++;
		}
		else{
			return false;
		}
	}
}

unsigned long hash(char* s, int modulus){ //djb2 by Dan Bernstein, modified to use lowercase
	char* str = s;
    unsigned long hash = 15485863;
    int c;
    while ((c = *str++)){
    	if (c>=65 && c<=90){//Convert to lowercase
    		c+=32;
    	}
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */\
        if (hash>=modulus){
        	hash%=modulus;
        }
    }
    return (hash);
}

long searchTable(char* name, struct Page** page_hash_table, long int table_size){
	long index=hash(name, table_size);
	//long int steps=0;
	while (true){
		if (page_hash_table[index]==NULL){
			return -1;
		}
		if (caseless_equal((page_hash_table[index])->name,name)){
			return index;
		}
		index=(1+index)%table_size;
		//steps++;
	}
}

long addToTable(struct Page* page, struct Page** page_hash_table, long int table_size){
	long index=hash(page->name, table_size);
	while (true){
		if (page_hash_table[index]==NULL){
			page_hash_table[index]=page;
			return index;
		}
		index=(1+index)%table_size;
	}
}



void free_page(struct Page* p){
	if (p->num_links>0){
		free(p->links);
	}
	free(p->name);
	free(p);
	p=NULL;
}


char* readn(FILE* file, int n){
	char* s = malloc(n+1); s[n]='\0';
	fread(s, sizeof(char), n, file);
	return s;
}

void trunctate_start(char *s, size_t n){
    size_t len = strlen(s);
    if (n > len)
        return;  // Or: n = len;
    memmove(s, s+n, len - n + 1);
}

struct Page** load_page_hash_table(long int* table_size, char* table_path){
	FILE* read_file=fopen(table_path, "rb");
	fread(table_size, sizeof(table_size[0]), 1, read_file);
	fgetc(read_file);
	printf("Table size: %li\n", *table_size);
	struct Page* current_page;
	int str_len;
	struct Page** table = malloc(sizeof(struct Page*) * (*table_size));
	for (long int i=0;i<(*table_size);i++){
		fread(&str_len, sizeof(str_len), 1, read_file);
		if (str_len>0){
			current_page=malloc(sizeof(struct Page));
			current_page->name=readn(read_file, str_len);
			//printf("New name: %s\n", current_page->name);

			fread(&current_page->num_links, sizeof(int), 1, read_file);
			//printf("%i\n", current_page->num_links);
			if (current_page->num_links>0){
				current_page->links=malloc(sizeof(long int) * current_page->num_links);
				fread(current_page->links, sizeof(long int), current_page->num_links, read_file);
			}
			else{
				current_page->links=NULL;
			}
			fread(&current_page->is_redirect, sizeof(bool), 1, read_file);
			table[i]=current_page;
			//printf("Num links: %i\n", current_page->num_links);
		}
		else{
			table[i]=NULL;
		}
		fgetc(read_file);
	}
	//printf("Table read finished.\n");
	return table;
}



char** deserialize_links(int read_pipe, int* num_links_read){
	int link_length;
	read(read_pipe,num_links_read,sizeof(*num_links_read)); //Read off number of links
	char** links_read=malloc((*num_links_read)*sizeof(char*)); // Allocate the space for each link read out
	for (int i=0;i<(*num_links_read);i++){ //For each link
		read(read_pipe, &link_length, sizeof(link_length)); //Read length
		links_read[i]=malloc(link_length+1); //Allocate space to hold the link
		read(read_pipe,links_read[i],link_length+1); //Read the link, including null terminator
	}
	return links_read;
}

void serialize_links(int write_pipe, int num_links, char** links){
	int link_length;
	write(write_pipe, &num_links, sizeof(num_links)); //Write number of strings to the pipe
	for (int i=0;i<num_links;i++){ //For each string
		link_length=strlen(links[i]);
		write(write_pipe, &link_length, sizeof(link_length)); //Write its length
		write(write_pipe,links[i],link_length+1); //Write the string, including null terminator
		//printf("Link: %s\n", final_links[i]);
	}
}

int void_compare_long(const void* a, const void* b){
	if ((*((int*) a))==(*((int*) b))){
		return 0;
	}
	else if ((*((long *) a))<(*((long *) b))){
		return -1;
	}
	else{
		return 1;
	}
}




int main(int argc, char* argv[]){
	if (argc==1){
		printf("No arguments passed!\n");
		exit(0);
	}
	if (strcmp(argv[1],"build")==0){
		if (argc!=3 && argc!=5){
			fprintf(stderr, "Usage: ./wikigraph build \"source_path\" (-o \"out_path\")\n");
			exit(-1);
		}



		int nontabler_PID;
		int start_end_pipe[2];
		pipe(start_end_pipe);
		if ((nontabler_PID=fork())<0){ //Fork. If error:
			fprintf(stderr, "Fork failed!\n");
			exit(-1);
		}
		else if (nontabler_PID==0){ //The child reads the file and passes it out to its children. It also passes pipes from its children back to its parent, the table-writer
			close(start_end_pipe[0]);//Close the read end of the pipe immediately.

			int reader_PID; //Make a pipe to pipe between the reader and the link processor
			int reader_processor_pipe[2];
			pipe(reader_processor_pipe);

			if ((reader_PID=fork())<0){
				fprintf(stderr, "Reader-processor fork failed!\n");
				exit(-1);
			}
			else if (reader_PID==0){//If we are the reader:
				close(reader_processor_pipe[0]);
				close(start_end_pipe[1]); //We don't need access to the table-writer, so get rid of this

				FILE* source_file;
				if ((source_file=fopen(argv[2], "r"))==NULL){
					fprintf(stderr, "Opening source file (%s) failed!", argv[2]);
					exit(-1);
				}

				struct CBuf* buf = malloc(sizeof(struct CBuf)); memset(buf->buffer,0,10); buf->current_index=0;
				int c;
				char* title=NULL;
				bool in_page=false;
				int num_links=0;
				int max_links;
				char** page_links; //We presume that there will be no more than this many links in any page. Fixable if needed
				bool is_redirect;

				long char_count=0;

				clock_t t=clock();
				long num_pages=0;
				while ((c=fgetc(source_file))!=EOF){ // && num_pages<10000
					char_count++;
					write_char(buf, c);
					if (!in_page){
						if (just_read(buf, "<page>", 6)){
							in_page=true;
							num_links=0;
							max_links=32;
							page_links=malloc(max_links*sizeof(char*));
							is_redirect=false;
						}
					}
					else{
						if (just_read(buf, "<title>", 7) && title==NULL){
							title=read_until(source_file, "</title>",8);//Read off the title
							if (strncmp("File:",title,5)==0 || strncmp(title, "Image:", 6)==0){
								free(title);
								title=NULL;
								for (int i=0; i<num_links; i++){
									free(page_links[i]);
								}
								free(page_links);
								in_page=false;
							}
						}
						if (just_read(buf, "#REDIRECT", 9)){
							is_redirect=true;
						}

						if (just_read(buf, "[[", 2)){ //If we read the start of a link. Second clause is to ignore XML typos in the source with 3+ brackets
							page_links[num_links]=read_until(source_file,"]]",2); //Read out until the end of it
							num_links++;
							if (num_links==max_links){
								max_links+=32;
								page_links=realloc(page_links, max_links*sizeof(char*));
							}
						}
						if (just_read(buf, "</page>", 7)){ //If we finished a page, write out
							num_pages++;
							int link_length;
							if (title!=NULL){
								link_length=strlen(title);
								write(reader_processor_pipe[1], &link_length, sizeof(link_length)); //Write title length
								write(reader_processor_pipe[1],title,link_length+1); //Write title
								serialize_links(reader_processor_pipe[1], num_links, page_links);
								write(reader_processor_pipe[1],&is_redirect,sizeof(is_redirect));
							}
							in_page=false;

							for (int i=0; i<num_links; i++){//We free all the pages at the end, no matter when they were deprocessed
								free(page_links[i]);
							}
							free(page_links);
							free(title);
							title=NULL;
						}
					}
				}
				printf("Reader processor time: %f\n", ((float)clock()-t)/CLOCKS_PER_SEC);
				exit(0); //Exit so that child can detect
			}
			else{//If we are the processor, we read links from the reader, truncate them, check for duplicates, and send the shortened list back
				close(reader_processor_pipe[1]);
				bool reader_alive=true;

				clock_t t = clock();
				while (reader_alive || poll(&(struct pollfd){ .fd = reader_processor_pipe[0], .events = POLLIN }, 1, 0)==1){ //While the reader is alive or there is data lingering in the pipe
					int name_length=-1; char* name;
					read(reader_processor_pipe[0],&name_length,sizeof(name_length)); //Read off number of links
					if (name_length==-1){//This implies that the pipe has been closed. Exit.
						break;
					}
					name=malloc(name_length+1);
					read(reader_processor_pipe[0],name,name_length+1);
					//printf("Length: %i. Name: %s\n", name_length, name);
					int num_links_read;
					char** links_read=deserialize_links(reader_processor_pipe[0], &num_links_read);
					bool is_redirect; read(reader_processor_pipe[0],&is_redirect,sizeof(is_redirect));

					for (int i=0;i<num_links_read;i++){
						while (links_read[i][0]=='['){ //Delete any leading brackets
							int l=strlen(links_read[i]);
							char* nl=malloc(l);
							memcpy(nl,links_read[i]+1,l);
							free(links_read[i]);
							links_read[i]=nl;
						}
						if (strchr(links_read[i],'|')!=NULL){ //If the link contains a bar (indicating a page alias), cut off the bar and anything past
							long index=strchr(links_read[i],'|')-links_read[i];
							links_read[i]=realloc(links_read[i], index+1);
							links_read[i][index]='\0';
						}
						if (strchr(links_read[i],'#')!=NULL){ //If the link contains a hashtag (referencing a specific part of the page), cut it off
							long index=strchr(links_read[i],'#')-links_read[i];
							links_read[i]=realloc(links_read[i], index+1);
							links_read[i][index]='\0';
						}
					}
					for (int i=0;i<num_links_read;i++){//Correct the first letter of each link to uppercase in advance of sorting
						if (links_read[i][0]<=122 && links_read[i][0]>=97){
							links_read[i][0]-=32;
						}
					}

					qsort(links_read, num_links_read, sizeof(char*), (int(*)(const void *, const void *))&strcmp);
					int num_distinct_links=0;
					char** final_links=malloc(num_links_read*sizeof(char*));
					for (int i=0; i<num_links_read;i++){
						if (num_distinct_links==0 || !caseless_equal(links_read[i], final_links[num_distinct_links-1])){
							if ((strncmp(links_read[i], "File:", 5) != 0) && (strncmp(links_read[i], "Image:", 6) != 0)){
								final_links[num_distinct_links]=links_read[i];
								num_distinct_links++;
							}
						}
					}

					int link_length;
					link_length=strlen(name);
					write(start_end_pipe[1], &link_length, sizeof(link_length)); //Write title length
					write(start_end_pipe[1],name,link_length+1); //Write title
					serialize_links(start_end_pipe[1], num_distinct_links, final_links);
					write(start_end_pipe[1],&is_redirect,sizeof(is_redirect));



					for (int i=0; i<num_links_read; i++){//We free all the pages at the end, no matter when they were deprocessed
						free(links_read[i]);
					}
					free(links_read);
					free(final_links);
					//printf("Title: %s\n", name);
					free(name);
					int status;
					if (waitpid(reader_PID, &status, WNOHANG)==reader_PID){//If the reader terminates, note it
						reader_alive=false;
						printf("Finished reading file!\n");
						if (status!=0){
							fprintf(stderr, "Reader exited with error!\n");
							exit(-1);
						}
					}
				}
				printf("Link processor processor time: %f\n", ((float)clock()-t)/CLOCKS_PER_SEC);
				exit(0);
			}
		}

		else{ //If we are the parent, then we will prepare to recieve transmissions from children
			clock_t t = clock();
			close(start_end_pipe[1]);//Close the write end of the pipe immediately.


			long int page_hash_table_size = 48000013; //Bigger than the current size of Wikipedia. Also prime.
			struct Page** page_hash_table=malloc(page_hash_table_size*sizeof(struct Page*)); memset(page_hash_table, 0, page_hash_table_size*sizeof(struct Page*));
			bool* page_found=malloc(page_hash_table_size*sizeof(bool)); memset(page_found, false, page_hash_table_size*sizeof(bool));
			long page_hash_table_spots_filled=0;
			long pages_processed=0;

			bool nontabler_alive=true;



			while ((nontabler_alive || poll(&(struct pollfd){ .fd = start_end_pipe[0], .events = POLLIN }, 1, 0)==1) && page_hash_table_spots_filled<page_hash_table_size*.99){ //The table is not filled and the processor is still giving us content
				int name_length=-1; char* name;
				read(start_end_pipe[0],&name_length,sizeof(name_length)); //Read off number of links
				if (name_length==-1){ //We tried to read and got 0 back. This implies that the pipe was closed
					break;
				}
				name=malloc(name_length+1);
				read(start_end_pipe[0],name,name_length+1);

				long table_loc = searchTable(name, page_hash_table, page_hash_table_size); //Find the name in the table. If it's not there, search for it.
				if (table_loc == -1){//If it wasn't found:
					struct Page* new_page = malloc(sizeof(struct Page)); new_page->num_links=0; new_page->links=NULL; new_page->name=name;
					table_loc = addToTable(new_page, page_hash_table, page_hash_table_size);
					page_hash_table_spots_filled++;
				}
				else{ //If it was found, correct the name (capitalization may be strange, as it was created from a link). Also correct redirect status
					free(page_hash_table[table_loc]->name);
					page_hash_table[table_loc]->name=name;
				}
				page_found[table_loc]=true;


				int num_links_read;
				char** links_read=deserialize_links(start_end_pipe[0], &num_links_read);
				bool is_redirect; read(start_end_pipe[0],&is_redirect,sizeof(is_redirect));
				page_hash_table[table_loc]->is_redirect=is_redirect;


				long* link_locs=malloc(num_links_read*sizeof(long)); //Allocate space for numeric links  //////MEMORY LEAK HERE (SPORADIC)

				for (int i=0;i<num_links_read;i++){ //For each link
					link_locs[i]=searchTable(links_read[i], page_hash_table, page_hash_table_size); //See if the link is there.
					if (link_locs[i]==-1){//If we came up blank, make a new page for that link.
						struct Page* new_page = malloc(sizeof(struct Page)); new_page->num_links=0; new_page->links=NULL; new_page->name=strdup(links_read[i]), new_page->is_redirect=false;
						link_locs[i] = addToTable(new_page, page_hash_table, page_hash_table_size);
						page_hash_table_spots_filled++;
					}
					free(links_read[i]);
				}
				free(links_read);

				page_hash_table[table_loc]->name=name;
				page_hash_table[table_loc]->num_links=num_links_read;
				page_hash_table[table_loc]->links=link_locs;
				pages_processed++;
				if (pages_processed%1000==0){
					printf("%li pages processed. %li spots filled.\n", pages_processed, page_hash_table_spots_filled);
				}

				int status;
				if (waitpid(nontabler_PID, &status, WNOHANG)==nontabler_PID){//If the reader/processor terminates, note it
					nontabler_alive=false;
					printf("Finished reading file!\n");
					if (status!=0){
						fprintf(stderr, "Nontabler exited with error!\n");
						exit(-1);
					}
				}
			}
			printf("Spots filled: %li. Nontabler alive: %i\n", page_hash_table_spots_filled, nontabler_alive);

			printf("Table writer processor time: %f\n",((float)(clock()-t))/CLOCKS_PER_SEC);

			long* is_redirect_to=malloc(sizeof(long) * page_hash_table_size);


			for (long i=0; i<page_hash_table_size; i++){//Free all non-found pages
				if (page_hash_table[i]!=NULL && !page_found[i]){
					free_page(page_hash_table[i]);
					page_hash_table[i]=NULL;
				}
			}
			for (long i=0; i<page_hash_table_size; i++){ //Put all redirects into table
				is_redirect_to[i]=-1;
				if (page_hash_table[i]!=NULL){
					if (page_hash_table[i]->is_redirect){//If it is a redirect, note what it is a redirect to. We assume the last link on the page is the redirect link.
						if (page_hash_table[i]->num_links>0){//If the page has a link
							if (page_hash_table[page_hash_table[i]->links[(page_hash_table[i]->num_links)-1]]!=NULL){//If the redirect points to a page, put it into the redirect table. Otherwise, pretend it was never there.
								is_redirect_to[i]=page_hash_table[i]->links[(page_hash_table[i]->num_links)-1];
							}							
						}
					}
				}
			}
			for (long i=0; i<page_hash_table_size; i++){ //Free all redirects. This is done separately from above due to redirect chains
				if (page_hash_table[i]!=NULL){
					if (page_hash_table[i]->is_redirect){//If it is a redirect, note what it is a redirect to. We assume the last link on the page is the redirect link.
						free_page(page_hash_table[i]);
						page_hash_table[i]=NULL;
					}
				}
			}
			


			for (long i=0; i<page_hash_table_size; i++){ //For any redirects that point to a redirect, make them point to whatever that redirect points to.
				if (is_redirect_to[i]!=-1){
					int count=0;
					while (is_redirect_to[is_redirect_to[i]]!=-1){//While our redirect points to a redirect
						is_redirect_to[i]=is_redirect_to[is_redirect_to[i]];
						count++;
						if (count==16){ //If we enter a long path of redirects, just assume the redirect is cyclic and ignore it.
							is_redirect_to[i]=-1;
							break;
						}
					}
					if (is_redirect_to[i]!=-1 && page_hash_table[is_redirect_to[i]]==NULL){//If we pointed to a previously removed redirect, just throw this one away.
						is_redirect_to[i]=-1;
					}
				}
			}
			printf("Completed redirect compression.\n");

			long num_pages_alive=0;

			for (long i=0;i<page_hash_table_size;i++){
				if (page_hash_table[i]!=NULL){
					num_pages_alive++;

					int num_live_links=0;
					long* live_links=malloc(sizeof(long)*page_hash_table[i]->num_links);

					qsort(page_hash_table[i]->links, page_hash_table[i]->num_links, sizeof(long), void_compare_long);

					for (int j=0; j<page_hash_table[i]->num_links; j++){ //For each link whose page we actually have read, keep it


						if (page_hash_table[page_hash_table[i]->links[j]]!=NULL){ //If the link is to a standard, alive page, keep it
							live_links[num_live_links]=page_hash_table[i]->links[j];
							num_live_links++;
						}
						else if (is_redirect_to[page_hash_table[i]->links[j]]!=-1){ //If the link was to a redirect, add whatever it pointed to instead
							if (page_hash_table[is_redirect_to[page_hash_table[i]->links[j]]]==NULL){
								fprintf(stderr, "Bad (likely doubled?) redirect!\n");
							}
							else{
								live_links[num_live_links]=is_redirect_to[page_hash_table[i]->links[j]];
								num_live_links++;
							}
							
						}
					}
					live_links=realloc(live_links, sizeof(long)*num_live_links);
					

					qsort(live_links, num_live_links, sizeof(long), void_compare_long);
					long* unique_links=malloc(sizeof(long)*num_live_links);
					int num_unique_links=0;
					for (int j=0; j<num_live_links;j++){
						if (page_hash_table[live_links[j]]==NULL){
							fprintf(stderr, "Link cleaning failed!\n");
						}
						if (j==0 || live_links[j]!=live_links[j-1]){
							unique_links[num_unique_links]=live_links[j];
							num_unique_links++;
						}
					}
					free(live_links);

					if (page_hash_table[i]->num_links>0){
						free(page_hash_table[i]->links); //Free old links
					}
					page_hash_table[i]->links=unique_links; //Add new ones
					page_hash_table[i]->num_links=num_unique_links; //Update number of links
				}
			}
			
			//At this point, we should have flushed all bad entries out of the table. This means that the hash table will no longer function, as the search-paths for various entries will be bad. We need to remake the table.

			long final_page_hash_table_size = 1.1*num_pages_alive;
			while (!isPrime(final_page_hash_table_size)){
				final_page_hash_table_size++;
			}


			struct Page** final_page_hash_table = malloc(sizeof(struct Page*)*final_page_hash_table_size); memset(final_page_hash_table, 0, sizeof(struct Page*)*final_page_hash_table_size);

			printf("Table purge complete. %li pages remaining. Shrinking table to length %li...\n", num_pages_alive, final_page_hash_table_size);

			long* moved_to_index=malloc(sizeof(long)*page_hash_table_size);//For any entry in the old page hash table, points to where it was moved.
			for (long i=0; i<page_hash_table_size; i++){//Add every page remaining in the page table to the final table
				if (page_hash_table[i]!=NULL){
					moved_to_index[i] = addToTable(page_hash_table[i], final_page_hash_table, final_page_hash_table_size);
				}
			}

			printf("Downsized table. Relinking...");

			for (long i=0; i<final_page_hash_table_size; i++){//Correct every link in every page in the final table
				if (final_page_hash_table[i]!=NULL){
					for (int j=0; j<final_page_hash_table[i]->num_links; j++){
						if (page_hash_table[final_page_hash_table[i]->links[j]]==NULL){
							fprintf(stderr, "Found NULL link!\n");
						}
						final_page_hash_table[i]->links[j]=moved_to_index[final_page_hash_table[i]->links[j]];
					}
				}
			}


			//The entire file has now been read and processed. Time to write to the output file
			char* write_file_name;
			if (argc<5){
				printf("No storage filename selected. Defaulting to \"./table\"\n");
				write_file_name="./table";
			}
			else{
				write_file_name=argv[4];
			}
			FILE* write_file=fopen(write_file_name,"wb");
			fwrite(&final_page_hash_table_size, sizeof(long int), 1, write_file);//Write the table size to the file
			fputc('\n', write_file);
			int len;
			for (int i=0;i<final_page_hash_table_size;i++){
				if (final_page_hash_table[i]!=NULL){
					len=strlen(final_page_hash_table[i]->name);
					fwrite((const void *)&len, sizeof(int), 1, write_file);
					fputs(final_page_hash_table[i]->name, write_file);

					//printf("%s\n", page_hash_table[i]->name);

					fwrite((const void *)&(final_page_hash_table[i]->num_links), sizeof(int), 1, write_file);
					fwrite((const void *)(final_page_hash_table[i]->links), sizeof(long int), final_page_hash_table[i]->num_links, write_file);
					fwrite((const void *)&final_page_hash_table[i]->is_redirect, sizeof(bool), 1, write_file);
					free_page(final_page_hash_table[i]);
				}
				else{
					len=0;
					fwrite((const void *)&len, sizeof(int), 1, write_file);
				}
				fputc('\n', write_file);//Terminate every line of the file with a newline character
			}
			fclose(write_file);
			printf("Finished writing file!\n");
			free(final_page_hash_table);
			free(page_hash_table);
		}	
	}	

	else if (strcmp(argv[1], "pagestats")==0){
		if (argc!=3 && argc!=5){
			fprintf(stderr, "Could not process command. Usage: wikigraph pagestats \"pagename\" (-table \"tablepath\")");
			exit(-1);
		}
		char* table_path;
		if (argc==3){
			table_path="./table";
		}
		else{
			table_path=argv[4];
		}
		long int page_hash_table_size; struct Page** page_hash_table=load_page_hash_table(&page_hash_table_size, table_path);

		long int loc = searchTable(argv[2], page_hash_table, page_hash_table_size);
		if (loc==-1){
			fprintf(stderr, "%s not found!\n", argv[2]);
			exit(-1);
		}

		printf("Page found! Contains %i valid links, as listed below:\n", page_hash_table[loc]->num_links);
		char** linked_pages=malloc(sizeof(char*) * page_hash_table[loc]->num_links);
		for (int i=0;i<page_hash_table[loc]->num_links;i++){
			linked_pages[i]=page_hash_table[page_hash_table[loc]->links[i]]->name;
		}
		qsort(linked_pages, page_hash_table[loc]->num_links, sizeof(char*), (int(*)(const void *, const void *))&strcmp);
		for (int i=0;i<page_hash_table[loc]->num_links;i++){
			printf("%s\n", linked_pages[i]);
		}
	}

	else if (strcmp(argv[1],"stats")==0){ //Arg parsing dirty atm. TODO: clean up, rigorize.
		if (argc!=2 && argc!=4){
			fprintf(stderr, "Could not process command. Usage: wikigraph stats (-table \"tablepath\")");
			exit(-1);
		}
		char* table_path;
		if (argc==2){
			table_path="./table";
		}
		else{
			table_path=argv[3];
		}
		long int page_hash_table_size; struct Page** page_hash_table=load_page_hash_table(&page_hash_table_size, table_path);

		long total_pages=0;
		long total_links=0;
		long total_dead_links=0;


		for (long i=0;i<page_hash_table_size;i++){
			if (i%10000==0){
				printf("Checked %li indices out of %li.\n", i, page_hash_table_size);
			}
			//printf("Page: %s\n", page_hash_table[i]->name);
			if (page_hash_table[i]!=NULL){
				total_pages++;
				total_links+=page_hash_table[i]->num_links;
				for (int j=0; j<page_hash_table[i]->num_links; j++){
					if (page_hash_table[page_hash_table[i]->links[j]]==NULL){
						total_dead_links++;
					}
				}
			}
		}
		printf("Total pages: %li\n", total_pages);
		printf("Total links: %li\n", total_links);
		printf("Average links per page: %f\n", (float)total_links/total_pages);
		//printf("Total dead links: %li\n", total_dead_links);
	}


	else if (strcmp(argv[1],"path")==0){
		if (argc!=4 && argc!=6){ //Arg parsing dirty atm. TODO: clean up, rigorize.
			fprintf(stderr, "Could not process command. Usage: wikigraph path \"firstpage\" \"secondpage\" (-table \"tablepath\")");
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



		long int first_loc = searchTable(argv[2], page_hash_table, page_hash_table_size); long int second_loc = searchTable(argv[3], page_hash_table, page_hash_table_size);
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
			printf("No path found!\n");
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
	else if (strcmp(argv[1],"random")==0){
		if (argc!=2 && argc!=4){ //Arg parsing dirty atm. TODO: clean up, rigorize.
			fprintf(stderr, "Could not process command. Usage: wikigraph path \"firstpage\" \"secondpage\" (-table \"tablepath\")");
		}
		char* table_path;
		if (argc==2){
			table_path="./table";
		}
		else{
			table_path=argv[3];
		}
		long int page_hash_table_size; struct Page** page_hash_table=load_page_hash_table(&page_hash_table_size, table_path);
		srandom(clock());
		long index=random()%page_hash_table_size;
		while (page_hash_table[index]==NULL){
			index=random()%page_hash_table_size;
		}
		printf("%s\n", page_hash_table[index]->name);

		printf("%i links:\n", page_hash_table[index]->num_links);
		char** linked_pages=malloc(sizeof(char*) * page_hash_table[index]->num_links);
		for (int i=0;i<page_hash_table[index]->num_links;i++){
			linked_pages[i]=page_hash_table[page_hash_table[index]->links[i]]->name;
		}
		qsort(linked_pages, page_hash_table[index]->num_links, sizeof(char*), (int(*)(const void *, const void *))&strcmp);
		for (int i=0;i<page_hash_table[index]->num_links;i++){
			printf("%s\n", linked_pages[i]);
		}

		exit(0);
	}
	else{
		printf("Command not recognized!\n");
	}
}



