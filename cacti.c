#include "cacti.h"
#include "pool.h"
#include "list.h"
#include "actor.h"
#include "queue.h"
#include "error.h"
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
    fatal(sigprocmask(SIG_BLOCK, &mask, NULL));
    fatal(pthread_create(&sig_handler, NULL, &sig_wait, NULL));
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
    int sig;
    while (TRUE) {
        fatal(sigwait(&mask, &sig));
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
    actor_list = new_list(root);
    pool = new_pool(POOL_SIZE);
    int created_threads = create_threads(pool, actor_list);
    if (root == NULL
        || actor_list == NULL
        || pool == NULL
        || created_threads != SUCCESS) {
        if (root != NULL) {
            free(root->mailbox);
        }
        if (actor_list != NULL) {
            free(actor_list->start);
        }
        free(root);
        free(actor_list);
        free(pool);
        return FAILURE;
    }
    *actor = ROOT_ID;
    send_message(root->id, get_message(MSG_HELLO, 0, NULL));
    return SUCCESS;
}

void actor_system_join(actor_id_t actor) {
    if (find_actor(actor_list, actor) == NULL) {
        return;
    }
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
    if (actor_list == NULL) { // fixme
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
        pthread_mutex_unlock(&actor_ptr->mutex);
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
    if (actor_list == NULL) { // fixme
        return SYSTEM_HALTED_ERR;
    }
    actor_t *actor = find_actor_by_thread(actor_list, pthread_self());
    if (actor == NULL) {
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
