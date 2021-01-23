#ifndef CACTI_ACTOR_H
#define CACTI_ACTOR_H

#include "cacti.h"
#include "queue.h"
#include <pthread.h>

#define ALIVE 1
#define DEAD 2

#define OPERATED 1
#define IDLE 0

typedef struct actor {
    actor_id_t id;
    role_t *role;
    queue_t *mailbox; //todo: size limit
    pthread_mutex_t mutex;
    void *state;
    volatile pthread_t thread;
    volatile int condition;
    volatile int status;
} actor_t;

actor_t *new_actor(role_t *role, actor_id_t id);

#endif //CACTI_ACTOR_H
