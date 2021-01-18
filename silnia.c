#include "cacti.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>

#define MSG_FACTORIAL 1
#define MSG_WAKE_PARENT 2
#define NPROMTS 3

#define error(ret) fprintf(stderr, "Couldn't create system, error: %d\n", ret), exit(EXIT_FAILURE)

act_t *act;

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
            error(ret);
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
    message_t message = get_message(MSG_FACTORIAL, sizeof(ans_t), ans);
    int ret = send_message(send_to, message);
    if (ret != SUCCESS) {
        error(ret);
    }
}

void start_point(void **stateptr, size_t nbytes, void *data) {
    ans_t ans = *((ans_t*)data);
    if (ans.n == ans.k) {
        printf("finished, result: %d\n", ans.k_fact);
        return;
    }
    role_t *role = malloc(sizeof(role_t));
    role->nprompts = NPROMTS;
    role->prompts = act;
    actor_id_t actor = actor_id_self();
    state_t *my_state = *stateptr;
    my_state->ans.k_fact = ans.k_fact * (ans.k + 1);
    my_state->ans.k = ans.k + 1;
    my_state->ans.n = ans.n;
    //free(&ans);
    message_t message = get_message(MSG_SPAWN, sizeof(role_t), role);
    int ret = send_message(actor, message);
    if (ret != SUCCESS) {
        free(role);
        error(ret);
    }
}

void factorial(int n) {
    actor_id_t root;
    role_t *role = malloc(sizeof(role_t));
    role->nprompts = NPROMTS;
    role->prompts = act;
    int ret = actor_system_create(&root, role);
    if (ret != SUCCESS) {
        free(role);
        error(ret);
    }
    message_t message = get_message(MSG_HELLO, sizeof(actor_id_t), (void*)root);
    ret = send_message(root, message);
    if (ret != SUCCESS) {
        free(role);
        error(ret);
    }
    ans_t *ans = malloc(sizeof(ans_t));
    ans->k_fact = 1;
    ans->k = 0;
    ans->n = n;
    message = get_message(MSG_FACTORIAL, sizeof(ans_t), ans);
    ret = send_message(root, message);
    if (ret != SUCCESS) {
        free(role);
        error(ret);
    }
    actor_system_join(root);
}

int main(){
    int n;
    scanf("%d", &n);
    act_t acts[] = {hello, start_point, wait_child};
    act = acts;
    factorial(n);
	return 0;
}