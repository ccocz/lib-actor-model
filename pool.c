#include <stdio.h>
#include <signal.h>
#include "pool.h"

static void *worker(void *);
static void handle(actor_t *actor, message_t message, pool_t *pool);
static void handle_godie(actor_t *actor);
static void handle_spawn(actor_t *actor, message_t message, pool_t *pool);
static actor_t *find_ready_actor(size_t *last_pos,
                                 list_t *actor_list,
                                 message_t *message);
static void post_handling(actor_t *actor, pool_t *pool);
static void halt_workers(pool_t *pool);
static int wait_work(pool_t *pool);
static void send_hello(actor_id_t from, actor_id_t to);

pool_t *new_pool(size_t size) {
    pool_t *pool = malloc(sizeof(pool_t));
    if (pool == NULL) {
        return NULL;
    }
    pool->size = size;
    pool->keep_alive = TRUE;
    pool->alive_thread_cnt = 0;
    pool->work_cond_val = FALSE;
    pool->alive_actor_cnt = 1;
    pool->is_interrupted = FALSE;
    pool->is_destroyed = FALSE;
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->work_cond, NULL);
    pthread_cond_init(&pool->destroy_cond, NULL);
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
    pool->alive_thread_cnt = size;
    for (size_t i = 0; i < size; i++) {
        pthread_create(threads + i, NULL, &worker, pool);
    }
    return SUCCESS;
}

static void *worker(void *data) {
    pool_t *pool = (pool_t*)data;
    list_t *actor_list = pool->actor_list;
    size_t last_pos = -1;
    message_t message;
    actor_t *actor;
    while (1) {
        actor = find_ready_actor(&last_pos, actor_list, &message);
        if (actor != NULL) {
            handle(actor, message, pool);
        } else {
            if (wait_work(pool) == FALSE) {
                break;
            }
        }
    }
    pthread_mutex_lock(&pool->mutex);
    pool->alive_thread_cnt--;
    pthread_mutex_unlock(&pool->mutex);
    return NULL;
}

static int wait_work(pool_t *pool) {
    pthread_mutex_lock(&pool->mutex);
    while (pool->work_cond_val == FALSE) {
        pthread_cond_wait(&pool->work_cond, &pool->mutex);
    }
    pool->work_cond_val = FALSE;
    if (pool->keep_alive == FALSE) {
        pthread_mutex_unlock(&pool->mutex);
        return FALSE;
    }
    pthread_mutex_unlock(&pool->mutex);
    return TRUE;
}

static actor_t *find_ready_actor(size_t *last_pos,
                                 list_t *actor_list,
                                 message_t *message) {
    actor_t *actor;
    pthread_mutex_lock(&actor_list->mutex);
    for (size_t i = 0; i < actor_list->pos; i++) {
        *last_pos = (*last_pos + 1) % actor_list->pos;
        actor = actor_list->start[*last_pos];
        pthread_mutex_lock(&actor->mutex);
        if (actor->condition == IDLE && !is_empty(actor->mailbox)) {
            *message = pop(actor->mailbox);
            pthread_mutex_unlock(&actor_list->mutex);
            return actor;
        }
        pthread_mutex_unlock(&actor->mutex);
    }
    pthread_mutex_unlock(&actor_list->mutex);
    return NULL;
}

static void handle(actor_t *actor, message_t message, pool_t *pool) {
    actor->condition = OPERATED;
    actor->thread = pthread_self();
    pthread_mutex_unlock(&actor->mutex);
    switch (message.message_type) {
        case MSG_GODIE:
            handle_godie(actor);
            break;
        case MSG_SPAWN: {
            handle_spawn(actor, message, pool);
            break;
        }
        default:
            if (message.message_type >= 0
                && message.message_type < (long)actor->role->nprompts) {
                actor->role->prompts[message.message_type]
                        (&actor->state, message.nbytes, message.data);
            }
    }
    post_handling(actor, pool);
}

static void post_handling(actor_t *actor, pool_t *pool) {
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


static void handle_godie(actor_t *actor) {
    pthread_mutex_lock(&actor->mutex);
    actor->status = DEAD;
    pthread_mutex_unlock(&actor->mutex);
    /*pthread_mutex_lock(&pool->mutex);
    pool->alive_actor_cnt--;
    if (pool->alive_actor_cnt == 0) {
        pool->keep_alive = FALSE;
        pthread_cond_broadcast(&pool->destroy_cond); //fixme: wake only one
    }
    pthread_mutex_unlock(&pool->mutex);*/
}

static void handle_spawn(actor_t *actor, message_t message, pool_t *pool) {
    pthread_mutex_lock(&pool->mutex);
    if (pool->is_interrupted) {
        pthread_mutex_unlock(&pool->mutex);
        return;
    }
    pthread_mutex_unlock(&pool->mutex);
    role_t *role = (role_t*)message.data;
    actor_id_t id = add_actor(pool->actor_list, role);
    send_hello(actor->id, id);
    pthread_mutex_lock(&pool->mutex);
    pool->alive_actor_cnt++;
    pthread_mutex_unlock(&pool->mutex);
}

static void send_hello(actor_id_t from, actor_id_t to) {
    message_t hello;
    hello.message_type = MSG_HELLO;
    hello.nbytes = sizeof(actor_id_t);
    hello.data = (void*)from;
    send_message(to, hello);
}

void destroy_pool(pool_t *pool) {
    halt_workers(pool);
    for (size_t i = 0; i < pool->size; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->work_cond);
    pthread_cond_destroy(&pool->destroy_cond);
    free_list(pool->actor_list);
    free(pool->threads);
    free(pool);
}

static void halt_workers(pool_t *pool) {
    pthread_mutex_lock(&pool->mutex);
    while (pool->keep_alive == TRUE) {
        pthread_cond_wait(&pool->destroy_cond, &pool->mutex);
    }
    pthread_mutex_unlock(&pool->mutex);
    int alive_cnt;
    do {
        pthread_mutex_lock(&pool->mutex);
        alive_cnt = pool->alive_thread_cnt;
        pthread_mutex_unlock(&pool->mutex);
        if (alive_cnt > 0) {
            pthread_mutex_lock(&pool->mutex);
            pool->work_cond_val = TRUE;
            pthread_cond_broadcast(&pool->work_cond);
            pthread_mutex_unlock(&pool->mutex);
        }
    } while (alive_cnt > 0);
}
