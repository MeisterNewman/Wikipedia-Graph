// Taken from https://stackoverflow.com/questions/13905774/in-c-how-do-you-use-libcurl-to-read-a-http-response-into-a-string

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

size_t write_data(void *ptr, size_t size, size_t nmemb, struct url_data *data) {
    size_t index = data->size;
    size_t n = (size * nmemb);
    char* tmp;
    data->size += (size * nmemb);
    tmp = realloc(data->data, data->size + 1); /* +1 for '\0' */
    if(tmp) {
        data->data = tmp;
    }
    else {
        if(data->data) {
            free(data->data);
        }
        fprintf(stderr, "Failed to allocate memory.\n");
        return 0;
    }
    memcpy((data->data + index), ptr, n);
    data->data[data->size] = '\0';
    return size * nmemb;
}

char* getHTML(char* url) {
    CURL *curl;
    struct url_data data;
    data.size = 0;
    data.data = malloc(4096); /* reasonable size initial buffer */
    if(NULL == data.data) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return NULL;
    }

    data.data[0] = '\0';

    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n",  
                        curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);

    }
    return data.data;
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