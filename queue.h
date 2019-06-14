#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED

struct QNode{ 
    int key;
    struct QNode *next;
};
struct Queue{
    struct QNode *front, *rear;
};

struct QNode* newNode(long k);
struct Queue *createQueue();
void enqueue(struct Queue *q, long k);
struct QNode* dequeue(struct Queue *q);

#endif