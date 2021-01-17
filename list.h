#ifndef CACTI_LIST_H
#define CACTI_LIST_H

#include <stdlib.h>
#include <pthread.h>
#include "actor.h"

typedef struct list {
    size_t size;
    size_t pos;
    actor_t **start;
    pthread_mutex_t mutex;
} list_t;

list_t *new_list(actor_t *root);
actor_t *find_actor(list_t *list, actor_id_t id);
actor_id_t add_actor(list_t *list, role_t *role);
actor_t *find_actor_by_thread(list_t *list, pthread_t thread);
void free_list(list_t *list);


#endif //CACTI_LIST_H
