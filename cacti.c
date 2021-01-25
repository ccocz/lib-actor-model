#include "cacti.h"
#include "pool.h"
#include "list.h"
#include "actor.h"
#include "queue.h"
#include <signal.h>

list_t *actor_list; // volatile?
pool_t *pool;

static void sig_handler(int signal) {
    exit(1);
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
    struct sigaction sa;
    sigset_t mask;
    sigemptyset(&mask);
    sa.sa_flags = 0;
    sa.sa_handler = sig_handler;
    sa.sa_mask = mask;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        return FAILURE;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        return FAILURE;
    }
    return SUCCESS;
}

void actor_system_join(actor_id_t actor) {
    // non existing actor id
    if (pool != NULL ) {
        destroy_pool(pool);
        pool = NULL;
    }
}

int send_message(actor_id_t actor, message_t message) {
    actor_t *actor_ptr = find_actor(actor_list, actor);
    if (actor_ptr == NULL) {
        return ACTOR_NOT_PRESENT_ERR;
    }
    pthread_mutex_lock(&actor_ptr->mutex);
    if (actor_ptr->status == DEAD) {
#ifdef DEBUG
        fprintf(stderr, "DEAD DEAD DEAD");
        fflush(stderr);
#endif
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
    actor_t *actor = find_actor_by_thread(actor_list, pthread_self());
    if (actor == NULL) {
        // error ?
        return FAILURE;
    } else {
        return actor->id;
    }
}