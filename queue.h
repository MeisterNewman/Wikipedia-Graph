#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED

struct QNode{ 
    long key;
    struct QNode *next;
};
struct Queue{
    struct QNode *front, *rear;
};

struct QNode* newNode(long k);
struct Queue *createQueue();
void enqueue(struct Queue *q, long k);
struct QNode* dequeue(struct Queue *q);
int queueLength(struct Queue *q);

#endif