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
    pthread_mutex_t mutex;
    pthread_mutex_t cnt_mutex;
    pthread_mutex_t alive_mutex;
    pthread_cond_t work_cond;
    volatile int work_cond_val;
    volatile int alive_cnt;
    volatile int keep_alive;
} pool_t;

pool_t *new_pool(size_t size);
int create_threads(pool_t *pool, list_t *actor_list);
void destroy_pool(pool_t *pool);

#endif //CACTI_POOL_H
