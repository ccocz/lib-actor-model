#include "actor.h"
#include <stdlib.h>
#include <stdio.h>

actor_t *new_actor(role_t *role, actor_id_t id) {
    actor_t *actor = malloc(sizeof(actor_t));
    if (actor == NULL) {
        return NULL;
    }
    actor->role = role;
    actor->mailbox = new_queue();
    if (actor->mailbox == NULL) {
        return NULL;
    }
    actor->id = id;
    actor->status = ALIVE;
    actor->state = NULL;
    actor->condition = IDLE;
    pthread_mutex_init(&actor->mutex, NULL);
    return actor;
}