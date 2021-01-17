#include "pool.h"

//todo: check after malloc
//todo: syserr

static void *worker(void *);
static void handle(actor_t *actor, message_t message, list_t *actor_list);

pool_t *new_pool(size_t size) {
    pool_t *pool = malloc(sizeof(pool_t));
    if (pool == NULL) {
        return NULL;
    }
    pool->size = size;
    pool->keep_alive = TRUE;
    pool->alive_cnt = 0;
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
    int found = 0;
    message_t message;
    actor_t *actor = NULL;
    int on_wait = 0;
    while (!on_wait || found) {
        pthread_mutex_lock(&actor_list->mutex);
        found = 0;
        for (size_t i = 0; i < actor_list->size; i++) {
            actor = actor_list->start[last_pos];
            pthread_mutex_lock(&actor->mutex);
            if (!is_empty(actor->mailbox)) {
                message = pop(actor->mailbox);
                found = 1;
                break;
            }
            pthread_mutex_unlock(&actor->mutex);
            last_pos = (last_pos + 1) % actor_list->size;
        }
        pthread_mutex_unlock(&actor_list->mutex);
        if (found) {
            handle(actor, message, actor_list);
            pthread_mutex_unlock(&actor->mutex);
        } else {
            pthread_mutex_lock(&pool->mutex);
            while (pool->work_cond_val == FALSE) {
                pthread_cond_wait(&pool->work_cond, &pool->mutex);
            }
            pool->work_cond_val = FALSE;
            pthread_mutex_unlock(&pool->mutex);
        }
        pthread_mutex_lock(&pool->alive_mutex);
        on_wait |= !pool->keep_alive;
        pthread_mutex_unlock(&pool->alive_mutex);
    }
    pthread_mutex_lock(&pool->cnt_mutex);
    pool->alive_cnt--;
    pthread_mutex_unlock(&pool->cnt_mutex);
    return NULL;
}

static void handle(actor_t *actor, message_t message, list_t *actor_list) {
    actor->condition = OPERATED;
    actor->thread = pthread_self();
    switch (message.message_type) {
        case MSG_GODIE:
            actor->status = DEAD;
            break;
        case MSG_SPAWN: {
            role_t *role = (role_t *) message.data;
            actor_id_t id = add_actor(actor_list, role);
            message_t hello;
            hello.message_type = MSG_HELLO;
            hello.nbytes = sizeof(actor_id_t);
            hello.data = (void*)actor->id;
            send_message(id, hello);
            break;
        }
        default:
            actor->role->prompts[message.message_type]
            (&actor->state, message.nbytes, message.data);
    }
    actor->condition = IDLE;
}

void destroy_pool(pool_t *pool) {
    int alive_cnt;
    do {
        pthread_mutex_lock(&pool->cnt_mutex);
        alive_cnt = pool->alive_cnt;
        pthread_mutex_unlock(&pool->cnt_mutex);
        if (alive_cnt > 0) {
            pthread_cond_broadcast(&pool->work_cond);
        }
    } while (alive_cnt > 0);
    free_list(pool->actor_list);
    free(pool->threads);
    free(pool);
}