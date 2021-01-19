#include "cacti.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>

#define MSG_FACTORIAL 1
#define MSG_WAKE_PARENT 2
#define NPROMTS 3

role_t *factorial_role;

typedef struct ans {
    int k_fact;
    int k;
    int n;
} ans_t;

typedef struct state {
    ans_t ans;
} state_t;

message_t get_message(message_type_t type, size_t nbytes, void* data) {
    message_t message;
    message.message_type = type;
    message.nbytes = nbytes;
    message.data = data;
    return message;
}

void hello(void **stateptr, size_t nbytes, void *data) {
    *stateptr = malloc(sizeof(state_t));
    actor_id_t my_id = actor_id_self();
    if (my_id != ROOT_ID) {
        actor_id_t parent_id = (actor_id_t)data;
        message_t message = get_message(MSG_WAKE_PARENT, sizeof(actor_id_t), (void *) my_id);
        int ret = send_message(parent_id, message);
        if (ret != SUCCESS) {
            err("send message error ", ret);
        }
    }
}


void wait_child(void **stateptr, size_t nbytes, void *data) {
    actor_id_t send_to = (actor_id_t)data;
    state_t *my_state = *stateptr;
    ans_t *ans = malloc(sizeof(ans_t));
    ans->k_fact = my_state->ans.k_fact;
    ans->k = my_state->ans.k;
    ans->n = my_state->ans.n;
    free(*stateptr);
    message_t message = get_message(MSG_FACTORIAL, sizeof(ans_t), ans);
    int ret = send_message(send_to, message);
    if (ret != SUCCESS) {
        err("send_message error ", ret);
    }
}

void start_point(void **stateptr, size_t nbytes, void *data) {
    ans_t ans = *((ans_t*)data);
    if (ans.n == ans.k) {
        free(data);
        free(*stateptr);
        printf("finished, result: %d\n", ans.k_fact);
        fflush(stdout);
        return;
    }
    actor_id_t actor = actor_id_self();
    state_t *my_state = *stateptr;
    my_state->ans.k_fact = ans.k_fact * (ans.k + 1);
    my_state->ans.k = ans.k + 1;
    my_state->ans.n = ans.n;
    free(data);
    message_t message = get_message(MSG_SPAWN, sizeof(role_t), factorial_role);
    int ret = send_message(actor, message);
    if (ret != SUCCESS) {
        err("send message error ", ret);
    }
}

void factorial(int n) {
    actor_id_t root;
    int ret = actor_system_create(&root, factorial_role);
    if (ret != SUCCESS) {
        err("system create error ", ret);
    }
    message_t message = get_message(MSG_HELLO, sizeof(actor_id_t), (void*)root);
    ret = send_message(root, message);
    if (ret != SUCCESS) {
        err("send message error ", ret);
    }
    ans_t *ans = malloc(sizeof(ans_t));
    ans->k_fact = 1;
    ans->k = 0;
    ans->n = n;
    message = get_message(MSG_FACTORIAL, sizeof(ans_t), ans);
    ret = send_message(root, message);
    if (ret != SUCCESS) {
        err("send message error ", ret);
    }
    actor_system_join(root);
}

int main(){
    int n;
    scanf("%d", &n);
    act_t acts[] = {hello, start_point, wait_child};
    role_t *role = malloc(sizeof(role_t));
    role->nprompts = NPROMTS;
    role->prompts = acts;
    factorial_role = role;
    factorial(n);
    free(role);
	return 0;
}