#include <stdio.h>
#include "pool.h"

//todo: check after malloc
//todo: syserr

static void *worker(void *);
static void handle(actor_t *actor, message_t message, pool_t *pool);

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
    pthread_mutex_init(&pool->mutex, NULL);
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
    pool->alive_thread_cnt = size;
    return SUCCESS;
}

// todo : exit
static void *worker(void *data) {
    pool_t *pool = (pool_t*) data;
    list_t *actor_list = pool->actor_list;
    size_t last_pos = -1;
    int found;
    message_t message;
    actor_t *actor = NULL;
    while (1) {
#ifdef DEBUG
        fprintf(stderr, "starting_before, thread = %lu\n", pthread_self());
        fflush(stderr);
#endif
        pthread_mutex_lock(&actor_list->mutex);
        found = 0;
        for (size_t i = 0; i < actor_list->pos; i++) {
            last_pos = (last_pos + 1) % actor_list->pos;
            actor = actor_list->start[last_pos];
            //if (actor->condition == OPERATED) { // todo: tmp
                //continue;
            //}



            pthread_mutex_lock(&actor->mutex);
            if (actor->condition == IDLE) {
                if (!is_empty(actor->mailbox)) {
                    message = pop(actor->mailbox);
                    found = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&actor->mutex);



#ifdef DEBUG
            fprintf(stderr, "%lu waiting for actor %lu\n", pthread_self(), actor->id);
            fflush(stderr);
#endif

            //pthread_mutex_lock(&actor->mailbox->mutex); // one mutex
            //if (!is_empty(actor->mailbox)) {
                //message = pop(actor->mailbox);
                //found = 1;
                //pthread_mutex_unlock(&actor->mailbox->mutex);
                //break;
            //}
            //pthread_mutex_unlock(&actor->mailbox->mutex);

        }
        pthread_mutex_unlock(&actor_list->mutex);
        if (found) {

            //pthread_mutex_lock(&actor->mutex);
            //while (actor->condition == OPERATED) {
                //pthread_cond_wait(&actor->worker, &actor->mutex);
            //}

            actor->condition = OPERATED;
            actor->thread = pthread_self();
            pthread_mutex_unlock(&actor->mutex);
            handle(actor, message, pool);


#ifdef DEBUG
            fprintf(stderr, "finished, thread = %lu\n", pthread_self());
            fflush(stderr);
#endif
        } else {
            int shutdown = 0;
            pthread_mutex_lock(&pool->mutex);
            shutdown |= !pool->keep_alive;
            pthread_mutex_unlock(&pool->mutex);
            if (shutdown) {
                break;
            }
            pthread_mutex_lock(&pool->mutex);
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
    pthread_mutex_lock(&pool->mutex);
    pool->alive_thread_cnt--;
    pthread_mutex_unlock(&pool->mutex);
#ifdef DEBUG
    fprintf(stderr, "finishing %lu\n", pthread_self());
    fflush(stderr);
#endif
    return NULL;
}

static void handle(actor_t *actor, message_t message, pool_t *pool) {
#ifdef DEBUG
    fprintf(stderr, "%lu handles, type = %lu, hell = %lu\n", pthread_self(), message.message_type, MSG_SPAWN);
    fflush(stderr);
#endif
    switch (message.message_type) {
        case MSG_GODIE:
            pthread_mutex_lock(&actor->mutex);
            actor->status = DEAD;
            pthread_mutex_unlock(&actor->mutex);
            pthread_mutex_lock(&pool->mutex);
            pool->alive_actor_cnt--;
            if (pool->alive_actor_cnt == 0) {
                pool->keep_alive = FALSE;
            }
            pthread_mutex_unlock(&pool->mutex);
            break;
        case MSG_SPAWN: {
            role_t *role = (role_t *) message.data;
#ifdef DEBUG
            fprintf(stderr, "%lu adding with actor %lu\n", pthread_self(), actor->id);
            fflush(stdout);
#endif
            actor_id_t id = add_actor(pool->actor_list, role);
#ifdef DEBUG
            fprintf(stderr, "%lu created %lu\n", pthread_self(), id);
            fflush(stdout);
#endif
            message_t hello;
            hello.message_type = MSG_HELLO;
            hello.nbytes = sizeof(actor_id_t);
            hello.data = (void*)actor->id;
            send_message(id, hello);
            pthread_mutex_lock(&pool->mutex);
            pool->alive_actor_cnt++;
            pthread_mutex_unlock(&pool->mutex);
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
    int empty_queue;
    pthread_mutex_lock(&actor->mutex);
    actor->condition = IDLE;
    empty_queue = is_empty(actor->mailbox) ? TRUE : FALSE;
    pthread_mutex_unlock(&actor->mutex);
    if (!empty_queue) {
        pthread_mutex_lock(&pool->mutex);
        pool->work_cond_val = TRUE;
        pthread_cond_broadcast(&pool->work_cond);
        pthread_mutex_unlock(&pool->mutex);
    }
    //pthread_cond_broadcast(&actor->worker);
}

void destroy_pool(pool_t *pool) {
    int alive_cnt;
    do {
        pthread_mutex_lock(&pool->mutex);
        alive_cnt = pool->alive_thread_cnt;
        pthread_mutex_unlock(&pool->mutex);
        //fprintf(stderr, "cnt = %d\n", alive_cnt);
        //fflush(stderr);
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
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->work_cond);
    free_list(pool->actor_list);
    free(pool->threads);
    free(pool);
}