#include "cacti.h"
#include "pool.h"
#include "list.h"
#include "actor.h"
#include "queue.h"
#include <signal.h>
#include <stdio.h>

list_t *actor_list;
pool_t *pool;
pthread_t sig_handler;

static void *sig_wait(__attribute__((unused)) void *arg);

__attribute__((constructor)) void init() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGKILL); // todo: remove these
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGUSR1);
    int ret = sigprocmask(SIG_BLOCK, &mask, NULL);
    if (ret != 0) {
        // halt
    }
    ret = pthread_create(&sig_handler, NULL, &sig_wait, NULL);
    if (ret != 0) {
        //halt
    }
}

 __attribute__((destructor)) void destroy() {
    pthread_kill(sig_handler, SIGUSR1);
    pthread_join(sig_handler, NULL);
}

static void *sig_wait(__attribute__((unused)) void *arg) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGKILL);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGUSR1);
    int res, sig;
    while (TRUE) {
        res = sigwait(&mask, &sig);
        if (res != 0) {
            printf("error");
        }
        if (sig == SIGUSR1) {
            return NULL;
        }
        pthread_mutex_lock(&pool->mutex);
        pool->is_interrupted = TRUE;
        pthread_mutex_unlock(&pool->mutex);
        message_t message = get_message(MSG_GODIE, 0, NULL);
        for (size_t i = 0; i < actor_list->pos; i++) {
            send_message(actor_list->start[i]->id, message);
        }
        actor_system_join(ROOT_ID);
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
    send_message(root->id, get_message(MSG_HELLO, 0, NULL));
    return SUCCESS;
}

void actor_system_join(actor_id_t actor) {
    (void)actor;
    // todo: non existing actor id
    if (pool != NULL) {
        pthread_mutex_lock(&pool->mutex);
        if (pool->is_destroyed == TRUE) {
            pthread_mutex_unlock(&pool->mutex);
            return;
        }
        pool->is_destroyed = TRUE;
        pthread_mutex_unlock(&pool->mutex);
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

message_t get_message(message_type_t type, size_t nbytes, void* data) {
    message_t message;
    message.message_type = type;
    message.nbytes = nbytes;
    message.data = data;
    return message;
}
