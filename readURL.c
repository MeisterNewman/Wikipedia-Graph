// HTML get code taken from https://curl.haxx.se/libcurl/c/getinmemory.html

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "readURL.h"

char* base_str="href=\"";
char* base_str_wiki="href=\"/wiki/";


char *strndup(const char *s, int n){//Not defined for reasons beyond my comprehension
    char *p = malloc(n+1);
    memcpy(p,s,n);
    p[n]='\0';
    return p;
}

struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp){
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(ptr == NULL) {
    /* out of memory! */ 
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

char* getHTML(char* url) {
    curl_global_init(CURL_GLOBAL_NOTHING);
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.size=0;
    chunk.memory = malloc(1);

    

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n",  
                        curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    return chunk.memory;
}

char** scanLinks(char* HTML, int* num_scanned){
    *num_scanned = 0;
    long htmlLen = strlen(HTML);
    long index = 0;
    char** links=malloc((1<<16)*sizeof(char*));
    long linkLen;
    //printf("HTML: %s\n\n",HTML);
    while (index<htmlLen-12){
        if (strncmp(&HTML[index],base_str_wiki,strlen(base_str_wiki))==0){
            index+=strlen(base_str);
            // printf("Char loc: %li. Base loc: %li\n",(long)strchr(&HTML[index],'"'),(long)&HTML[index]);
            linkLen=strchr(&HTML[index],'"')-&HTML[index];
            if (linkLen>0){
                links[(*num_scanned)]=strndup(&HTML[index],linkLen);
                //printf("Link found: %s\n",links[(*num_scanned)]);
                (*num_scanned)++;
            }
        }
        index++;
    }
    // printf("Num links found: %i\n",(*num_scanned));
    links = realloc(links,(*num_scanned)*sizeof(char*));

    return links;
}