#ifndef CACTI_H
#define CACTI_H

#include <stddef.h>

typedef long message_type_t;

//todo: limits

#define MSG_SPAWN (message_type_t)0x06057a6e
#define MSG_GODIE (message_type_t)0x60bedead
#define MSG_HELLO (message_type_t)0x0

#ifndef ACTOR_QUEUE_LIMIT
#define ACTOR_QUEUE_LIMIT 1024
#endif

#ifndef CAST_LIMIT
#define CAST_LIMIT 1048576
#endif

//#define DEBUG 1
//#define DEBUG_LIGHT 1

#ifndef POOL_SIZE
#define POOL_SIZE 3
#endif

#define ROOT_ID 0

#define SYSTEM_HALTED_ERR -4
#define ACTOR_QUEUE_LIMIT_ERR -3
#define ACTOR_NOT_PRESENT_ERR -2
#define ACTOR_NOT_ALIVE_ERR -1

#define SUCCESS 0
#define FAILURE -1

typedef struct message
{
    message_type_t message_type;
    size_t nbytes;
    void *data;
} message_t;

typedef long actor_id_t;

actor_id_t actor_id_self();

typedef void (*const act_t)(void **stateptr, size_t nbytes, void *data);

typedef struct role
{
    size_t nprompts;
    act_t *prompts;
} role_t;

int actor_system_create(actor_id_t *actor, role_t *const role);

void actor_system_join(actor_id_t actor);

int send_message(actor_id_t actor, message_t message);

#endif