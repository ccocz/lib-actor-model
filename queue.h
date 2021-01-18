#ifndef CACTI_QUEUE_H
#define CACTI_QUEUE_H

#include "cacti.h"
#include <pthread.h>

typedef struct qnode{
    message_t message;
    struct qnode *next;
} qnode_t;

typedef struct queue{
    qnode_t *head;
    qnode_t *tail;
    pthread_mutex_t mutex;
} queue_t;

queue_t *new_queue(void);
void push(queue_t *queue, message_t message);
message_t pop(queue_t *queue);
int is_empty(queue_t *queue);
void free_queue(queue_t *queue);

#endif //CACTI_QUEUE_H
