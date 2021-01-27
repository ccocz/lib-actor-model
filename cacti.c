#include "cacti.h"
#include "pool.h"
#include "list.h"
#include "actor.h"
#include "queue.h"
#include <signal.h>
#include <stdio.h>

list_t *actor_list; // volatile?
pool_t *pool;

static void *sig_wait(__attribute__((unused)) void *arg) {
    pthread_detach(pthread_self());
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGKILL);
    sigaddset(&mask, SIGTERM);
    int res, sig;
    while (TRUE) {
        res = sigwait(&mask, &sig);
        if (res != 0) {
            printf("error");
        }
        pthread_mutex_lock(&pool->mutex);
        pool->is_interrupted = TRUE;
        pthread_mutex_unlock(&pool->mutex);
        message_t message;
        message.message_type = MSG_GODIE;
        message.data = NULL;
        message.nbytes = 0;
        for (size_t i = 0; i < actor_list->pos; i++) {
            send_message(actor_list->start[i]->id, message);
        }
        //actor_system_join(ROOT_ID);
        printf("Signal handling thread got signal %d\n", sig);
        fflush(stdout);
        return NULL;
    }
}

int actor_system_create(actor_id_t *actor, role_t *const role) {
    actor_t *root = new_actor(role, ROOT_ID);
    if (root == NULL) {
        return FAILURE;
    }
    actor_list = new_list(root);
    if (actor_list == NULL) {
        return FAILURE;
    }
    pool = new_pool(POOL_SIZE);
    if (pool == NULL) {
        return FAILURE;
    }
    int ret = create_threads(pool, actor_list);
    if (ret != SUCCESS) {
        return FAILURE;
    }
    *actor = ROOT_ID;
    pthread_t signal_waiter;
    ret = pthread_create(&signal_waiter, NULL, &sig_wait, NULL);
    if (ret != 0) {
        return FAILURE;
    }
    return SUCCESS;
}

void actor_system_join(actor_id_t actor) {
    // non existing actor id
    if (pool != NULL) {
        destroy_pool(pool);
        pool = NULL;
        actor_list = NULL;
    }
}

int send_message(actor_id_t actor, message_t message) {
    if (actor_list == NULL) {
        return SYSTEM_HALTED_ERR;
    }
    actor_t *actor_ptr = find_actor(actor_list, actor);
    if (actor_ptr == NULL) {
        return ACTOR_NOT_PRESENT_ERR;
    }
    pthread_mutex_lock(&actor_ptr->mutex);
    if (actor_ptr->status == DEAD) {
        pthread_mutex_unlock(&actor_ptr->mutex);
        return ACTOR_NOT_ALIVE_ERR;
    }
    if (actor_ptr->mailbox->size >= ACTOR_QUEUE_LIMIT) {
        return ACTOR_QUEUE_LIMIT_ERR;
    }
    push(actor_ptr->mailbox, message);
    pthread_mutex_unlock(&actor_ptr->mutex);
    // todo: don't receive if not aplicable, i.e., invalid request
    pthread_mutex_lock(&pool->mutex);
    pool->work_cond_val = TRUE;
    pthread_cond_broadcast(&pool->work_cond);
    pthread_mutex_unlock(&pool->mutex);
    return SUCCESS;
}

actor_id_t actor_id_self() {
    if (actor_list == NULL) {
        return SYSTEM_HALTED_ERR;
    }
    actor_t *actor = find_actor_by_thread(actor_list, pthread_self());
    if (actor == NULL) {
        // error ?
        return FAILURE;
    } else {
        return actor->id;
    }
}