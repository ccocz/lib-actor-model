#include "list.h"

list_t *new_list(actor_t *root) {
    list_t *list = malloc(sizeof(list_t));
    if (list == NULL) {
        return NULL;
    }
    list->size = 1;
    list->start = malloc(sizeof(actor_t*));
    if (list->start == NULL) {
        return NULL;
    }
    *list->start = root;
    list->pos = 1;
    pthread_mutex_init(&list->mutex, NULL);
    return list;
}

actor_t *find_actor(list_t *list, actor_id_t id) {
    pthread_mutex_lock(&list->mutex);
    actor_t *actor = NULL;
    if (id < (long)list->pos) {
        actor = list->start[id];
    }
    pthread_mutex_unlock(&list->mutex);
    return actor;
}

actor_id_t add_actor(list_t *list, role_t *role) {
    pthread_mutex_lock(&list->mutex);
    if (list->pos == list->size) {
        list->size *= 2;
        list->start = realloc(list->start,
                              list->size * sizeof(actor_t*)); //todo check
    }
    actor_id_t id = list->pos++;
    actor_t *actor = new_actor(role, id);
    list->start[id] = actor;
    pthread_mutex_unlock(&list->mutex);
    return id;
}

actor_t *find_actor_by_thread(list_t *list, pthread_t thread) {
    pthread_mutex_lock(&list->mutex);
    int match = FALSE;
    actor_t *actor = NULL;
    for (size_t i = 0; i < list->pos; i++) {
        actor = list->start[i];
        pthread_mutex_lock(&actor->mutex);
        if (actor->condition == OPERATED
            && pthread_equal(thread, actor->thread)) {
            match = TRUE;
        }
        pthread_mutex_unlock(&actor->mutex);
        if (match == TRUE) {
            break;
        }
    }
    pthread_mutex_unlock(&list->mutex);
    return actor;
}

void free_list(list_t *list) {
    for (size_t i = 0; i < list->pos; i++) {
        pthread_mutex_destroy(&list->start[i]->mutex);
        free_queue(list->start[i]->mailbox);
        free(list->start[i]);
    }
    free(list->start);
    pthread_mutex_destroy(&list->mutex);
    free(list);
}
/*
 * static void post_handling(actor_t *actor, pool_t *pool) {
    pthread_mutex_lock(&actor->mutex);
    actor->condition = IDLE;
    if (!is_empty(actor->mailbox)) {
        pthread_mutex_unlock(&actor->mutex);
        pthread_mutex_lock(&pool->mutex);
        pool->work_cond_val = TRUE;
        pthread_cond_broadcast(&pool->work_cond);
        pthread_mutex_unlock(&pool->mutex);
    } else {
        int status = actor->status;
        pthread_mutex_unlock(&actor->mutex);
        if (status == DEAD) {
            pthread_mutex_lock(&pool->mutex);
            pool->alive_actor_cnt--;
            if (pool->alive_actor_cnt == 0) {
                pool->keep_alive = FALSE;
                pthread_cond_broadcast(&pool->destroy_cond); //fixme: wake only one
            }
            pthread_mutex_unlock(&pool->mutex);
        }
    }
}

 */