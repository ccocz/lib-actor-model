#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

//todo: lock
queue_t *new_queue() {
    queue_t *queue = malloc(sizeof(queue_t));
    if (queue == NULL) {
        return NULL;
    }
    queue->head = NULL;
    queue->tail = NULL;
    return queue;
}

void push(queue_t *queue, message_t message) {
    //todo: message alloc
    qnode_t *new_message = malloc(sizeof(qnode_t));
    new_message->message = message;
    new_message->next = NULL;
    if (queue->head == NULL) {
        queue->head = new_message;
        queue->tail = new_message;
    } else {
        queue->tail->next = new_message;
        queue->tail = new_message;
    }
}

message_t pop(queue_t *queue) {
    message_t message = queue->head->message;
    qnode_t *tb_free = queue->head;
    queue->head = queue->head->next;
    free(tb_free);
    return message;
}

int is_empty(queue_t *queue) {
    return queue->head == NULL;
}

void free_queue(queue_t *queue) {
    while (!is_empty(queue)) {
        qnode_t *tb_free = queue->head;
        queue->head = queue->head->next;
        free(tb_free);
    }
}