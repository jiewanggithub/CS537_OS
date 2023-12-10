#include "safequeue.h"

void initPriorityQueue(PriorityQueue *pq, int capacity) {
    pq->head = NULL;
    pq->size = 0;
    pq->capacity = capacity;
    pthread_mutex_init(&pq->lock, NULL);
    pthread_cond_init(&pq->not_empty, NULL);
}

int insertItem(PriorityQueue *pq, QueueItem *item) {
    pthread_mutex_lock(&pq->lock);

    if (pq->size >= pq->capacity) {
        pthread_mutex_unlock(&pq->lock);
        return -1;
    }

    if (pq->head == NULL || pq->head->priority < item->priority) {
        item->next = pq->head;
        pq->head = item;
    } else {
        QueueItem *current = pq->head;
        while (current->next != NULL && current->next->priority >= item->priority) {
            current = current->next;
        }
        item->next = current->next;
        current->next = item;
    }

    pq->size++;
    pthread_cond_signal(&pq->not_empty);
    pthread_mutex_unlock(&pq->lock);
    return 0;
}


QueueItem* removeItem(PriorityQueue *pq) {
    pthread_mutex_lock(&pq->lock);

    while (pq->size == 0) {
        pthread_cond_wait(&pq->not_empty, &pq->lock);
    }

    QueueItem *item = pq->head;
    pq->head = item->next;
    pq->size--;

    pthread_mutex_unlock(&pq->lock);
    return item;
}

void destroyPriorityQueue(PriorityQueue *pq) {
    pthread_mutex_lock(&pq->lock);

    QueueItem *current = pq->head;
    while (current != NULL) {
        QueueItem *next = current->next;
        free(current);
        current = next;
    }

    pthread_mutex_unlock(&pq->lock);
    pthread_mutex_destroy(&pq->lock);
    pthread_cond_destroy(&pq->not_empty);
}
