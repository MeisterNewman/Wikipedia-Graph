// Taken from https://www.geeksforgeeks.org/queue-set-2-linked-list-implementation/ with minor modification
#include <stdlib.h>
#include "queue.h"


struct QNode* newNode(long k){ 
    struct QNode *temp = (struct QNode*)malloc(sizeof(struct QNode));
    temp->key = k;
    temp->next = NULL;
    return temp;
}
struct Queue *createQueue(){ 
    struct Queue *q = (struct Queue*)malloc(sizeof(struct Queue));
    q->front = q->rear = NULL;
    return q;
}
void enqueue(struct Queue *q, long k){ 
    struct QNode *temp = newNode(k); 
    if (q->rear == NULL){ 
       q->front = q->rear = temp;
       return;
    }
    q->rear->next = temp;
    q->rear = temp;
} 
struct QNode* dequeue(struct Queue *q){ 
    if (q->front == NULL){
       return NULL;
    }
    struct QNode *temp = q->front;
    q->front = q->front->next;
    if (q->front == NULL){
       q->rear = NULL;
    }
    return temp;
}

int queueLength(struct Queue *q){
    if (q->front == NULL){
       return 0;
    }
    int l=1;
    struct QNode *temp=q->front;
    while (temp->next!=NULL){
        temp=temp->next;
        l++;
    }
    return l;
} 