#ifndef CACTI_POOL_H
#define CACTI_POOL_H

#include "list.h"

#define TRUE 1
#define FALSE 0

//todo: ints volatile
typedef struct pool {
    list_t *actor_list;
    pthread_t *threads;
    size_t size;
    pthread_cond_t work_cond;
    pthread_cond_t destroy_cond;
    pthread_mutex_t mutex;
    volatile int work_cond_val;
    volatile int alive_thread_cnt;
    volatile int alive_actor_cnt;
    volatile int keep_alive;
    volatile int is_interrupted;

} pool_t;

pool_t *new_pool(size_t size);
int create_threads(pool_t *pool, list_t *actor_list);
void destroy_pool(pool_t *pool);

#endif //CACTI_POOL_H
