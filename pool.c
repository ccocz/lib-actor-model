#include <stdio.h>
#include "pool.h"

//todo: check after malloc
//todo: syserr

static void *worker(void *);
static void handle(actor_t *actor, message_t message, list_t *actor_list);

volatile int glob = 0;

pool_t *new_pool(size_t size) {
    pool_t *pool = malloc(sizeof(pool_t));
    if (pool == NULL) {
        return NULL;
    }
    pool->size = size;
    pool->keep_alive = TRUE;
    pool->alive_cnt = 0;
    pool->work_cond_val = FALSE;
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_mutex_init(&pool->cnt_mutex, NULL);
    pthread_mutex_init(&pool->alive_mutex, NULL);
    pthread_cond_init(&pool->work_cond, NULL);
    return pool;
}

int create_threads(pool_t *pool, list_t *actor_list) {
    size_t size = pool->size;
    pthread_t *threads = malloc(sizeof(pthread_t) * size);
    if (threads == NULL) {
        return FAILURE;
    }
    pool->threads = threads;
    pool->actor_list = actor_list;
    for (size_t i = 0; i < size; i++) {
        pthread_create(threads + i, NULL, &worker, pool);
    }
    pool->alive_cnt = size;
    return SUCCESS;
}

// todo : exit
static void *worker(void *data) {
    pool_t *pool = (pool_t*) data;
    list_t *actor_list = pool->actor_list;
    size_t last_pos = 0;
    int found;
    message_t message;
    actor_t *actor = NULL;
    //pthread_mutex_lock(&pool->alive_mutex);
    //int on_wait = !pool->keep_alive;
    //pthread_mutex_unlock(&pool->alive_mutex);
    //fprintf(stderr, "starting %lu\n", pthread_self());
    //fflush(stderr);
    while (1) {
#ifdef DEBUG
        fprintf(stderr, "starting_before, thread = %lu\n", pthread_self());
        fflush(stderr);
        pthread_mutex_lock(&actor_list->mutex);
        fprintf(stderr, "starting_after, thread = %lu\n", pthread_self());
        fflush(stderr);
#endif
        found = 0;
        for (size_t i = 0; i < actor_list->size; i++) {
            actor = actor_list->start[last_pos];
            last_pos = (last_pos + 1) % actor_list->pos;
            if (actor->condition == OPERATED) { // todo: tmp
                continue;
            }
            pthread_mutex_lock(&actor->mutex);
            if (!is_empty(actor->mailbox)) {
                message = pop(actor->mailbox);
                found = 1;
                break;
            }
            pthread_mutex_unlock(&actor->mutex);
        }
        int ret = pthread_mutex_unlock(&actor_list->mutex);
#ifdef DEBUG
        if (ret == 0) {
            fprintf(stderr, "thread = %lu unlocked successfully\n", pthread_self());
            fflush(stderr);
        }
#endif
        if (found) {
            handle(actor, message, actor_list);
            pthread_mutex_unlock(&actor->mutex);
#ifdef DEBUG
            fprintf(stderr, "finished, thread = %lu\n", pthread_self());
            fflush(stderr);
            //if (message.message_type == 0) {
                //fprintf(stderr, "second operation, thread = %lu, %d\n", pthread_self(), pthread_mutex_trylock(&actor_list->mutex));
                //fflush(stderr);
            //}
            //fprintf(stderr, "trylock: %d\n", pthread_mutex_trylock(&actor_list->mutex));
            //fflush(stderr);
#endif
        } else {
            int on_wait = 0;
            pthread_mutex_lock(&pool->alive_mutex);
            on_wait |= !pool->keep_alive;
            pthread_mutex_unlock(&pool->alive_mutex);
            if (on_wait) {
                break;
            }
            pthread_mutex_lock(&pool->mutex);
            //fprintf(stderr, "waiting %lu\n", pthread_self());
            //fflush(stderr);
            while (pool->work_cond_val == FALSE) {
                pthread_cond_wait(&pool->work_cond, &pool->mutex);
            }
#ifdef DEBUG
            fprintf(stderr, "exit %lu\n", pthread_self());
            fflush(stderr);
#endif
            pool->work_cond_val = FALSE;
            pthread_mutex_unlock(&pool->mutex);
        }
    }
    pthread_mutex_lock(&pool->cnt_mutex);
    pool->alive_cnt--;
    pthread_mutex_unlock(&pool->cnt_mutex);
#ifdef DEBUG
    fprintf(stderr, "finishing %lu\n", pthread_self());
    fflush(stderr);
#endif
    return NULL;
}

static void handle(actor_t *actor, message_t message, list_t *actor_list) {
#ifdef DEBUG
    fprintf(stderr, "%lu handles, type = %lu, hell = %lu\n", pthread_self(), message.message_type, MSG_SPAWN);
    fflush(stderr);
#endif
    actor->condition = OPERATED;
    actor->thread = pthread_self();
    switch (message.message_type) {
        case MSG_GODIE:
            actor->status = DEAD;
            break;
        case MSG_SPAWN: {
            role_t *role = (role_t *) message.data;
            actor_id_t id = add_actor(actor_list, role);
#ifdef DEBUG
            fprintf(stderr, "%lu created %lu\n", pthread_self(), id);
            fflush(stdout);
#endif
            message_t hello;
            hello.message_type = MSG_HELLO;
            hello.nbytes = sizeof(actor_id_t);
            hello.data = (void*)actor->id;
            send_message(id, hello);
            break;
        }
        default:
#ifdef DEBUG
            fprintf(stderr, "sending_default\n");
            fflush(stdout);
#endif
            actor->role->prompts[message.message_type]
            (&actor->state, message.nbytes, message.data);
    }
    actor->condition = IDLE;
    //fprintf(stderr, "finito %lu\n", pthread_self());
    //fflush(stderr);
}

void destroy_pool(pool_t *pool) {
    int alive_cnt;
    do {
        pthread_mutex_lock(&pool->cnt_mutex);
        alive_cnt = pool->alive_cnt;
        pthread_mutex_unlock(&pool->cnt_mutex);
#ifdef DEBUG
        //fprintf(stderr, "cnt = %d\n", alive_cnt);
        //fflush(stderr);
#endif
        if (alive_cnt > 0) {
            pthread_mutex_lock(&pool->mutex);
            pool->work_cond_val = TRUE;
            pthread_cond_broadcast(&pool->work_cond);
            pthread_mutex_unlock(&pool->mutex);
        }
    } while (alive_cnt > 0);
    for (size_t i = 0; i < pool->size; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    free_list(pool->actor_list);
    free(pool->threads);
    free(pool);
}