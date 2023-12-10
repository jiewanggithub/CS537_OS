#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct QueueItem{
    int priority;
    int client_fd;
    char *method;
    char *path;
    int delay;
    struct QueueItem* next;
} QueueItem;

typedef struct {
    QueueItem* head;
    int size;
    int capacity;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} PriorityQueue;

#define QUEUE_FULL_ERROR -1


void initPriorityQueue(PriorityQueue *pq, int capacity);
int insertItem(PriorityQueue *pq, QueueItem *item);
QueueItem* removeItem(PriorityQueue *pq);
void destroyPriorityQueue(PriorityQueue *pq);