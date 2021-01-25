#include "list.h"
#include <stdio.h>

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
        list->start = realloc(list->start, list->size * sizeof(actor_t*)); //todo check
    }
    actor_id_t id = list->pos++;
    actor_t *actor = new_actor(role, id);
    list->start[id] = actor;
    pthread_mutex_unlock(&list->mutex);
    return id;
}

actor_t *find_actor_by_thread(list_t *list, pthread_t thread) {
#ifdef DEBUG
    fprintf(stderr, "%lu waiting for list mutex_cond\n", pthread_self());
#endif
    pthread_mutex_lock(&list->mutex);
#ifdef DEBUG
    fprintf(stderr, "%lu got list mutex_cond\n", pthread_self());
#endif
    int match = 0;
    actor_t *actor = NULL;
    for (size_t i = 0; i < list->pos; i++) {
        actor = list->start[i];
        pthread_mutex_lock(&actor->mutex);
        if (actor->condition == OPERATED
            && pthread_equal(thread, actor->thread)) {
            match = 1;
        }
        pthread_mutex_unlock(&actor->mutex);
        if (match) {
            break;
        }
    }
    pthread_mutex_unlock(&list->mutex);
#ifdef DEBUG
    fprintf(stderr, "not found!!!\n");
    fflush(stderr);
#endif
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