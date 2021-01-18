#include "actor.h"
#include <stdlib.h>
#include <stdio.h>

actor_t *new_actor(role_t *role, actor_id_t id) {
    actor_t *actor = malloc(sizeof(actor_t));
    if (actor == NULL) {
#ifdef DEBUG_LIGHT
        fprintf(stderr, "couldn't allocate actor");
        fflush(stderr);
#endif
        return NULL;
    }
    actor->role = role;
    actor->mailbox = new_queue();
    if (actor->mailbox == NULL) {
#ifdef DEBUG_LIGHT
        fprintf(stderr, "couldn't allocate mailbox");
        fflush(stderr);
#endif
        return NULL;
    }
    actor->id = id;
    actor->status = ALIVE;
    actor->state = NULL;
    actor->condition = IDLE; // ?
    pthread_mutex_init(&actor->mutex, NULL);
    pthread_cond_init(&actor->worker, NULL);
    return actor;
}