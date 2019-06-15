#ifndef TEST_H_INCLUDED
#define TEST_H_INCLUDED


struct url_data {
    size_t size;
    char* data;
};
size_t write_data(void *ptr, size_t size, size_t nmemb, struct url_data *data);
char* getHTML(char* url);
char** scanLinks(char* HTML, int* num_scanned);
char *strndup(const char *s, int n);
void readURLInit();
#endif