#include "cacti.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>

#define MSG_FACTORIAL 1
#define MSG_WAKE_PARENT 2
#define NPROMTS 3

role_t *factorial_role;
role_t *root_role;
int n;

typedef struct ans {
    unsigned long long k_fact;
    int k;
    int n;
    actor_id_t parent;
} ans_t;

typedef struct state {
    ans_t *ans;
} state_t;

void hello_root(__attribute__((unused)) void **stateptr,
                __attribute__((unused)) size_t nbytes,
                __attribute__((unused)) void *data) {
    ans_t *ans = malloc(sizeof(ans_t));
    ans->k_fact = 1;
    ans->k = 0;
    ans->n = n;
    ans->parent = actor_id_self();
    message_t message = get_message(MSG_FACTORIAL, sizeof(ans_t), ans);
    int ret = send_message(actor_id_self(), message);
    if (ret != SUCCESS) {
        err("send message error start point ", ret);
    }
}


void hello(__attribute__((unused)) void **stateptr,
           __attribute__((unused)) size_t nbytes,
           void *data) {
    actor_id_t my_id = actor_id_self();
    actor_id_t parent_id = (actor_id_t)data;
    message_t message = get_message(MSG_WAKE_PARENT, sizeof(actor_id_t), (void *) my_id);
    int ret = send_message(parent_id, message);
    if (ret != SUCCESS) {
        err("send message error hello", ret);
    }
}

void wait_child(void **stateptr, __attribute__((unused)) size_t nbytes, void *data) {
    actor_id_t send_to = (actor_id_t)data;
    state_t *my_state = *stateptr;
    ans_t *ans = my_state->ans;
    ans->parent = actor_id_self();
    free(*stateptr);
    message_t message = get_message(MSG_FACTORIAL, sizeof(ans_t), ans);
    int ret = send_message(send_to, message);
    if (ret != SUCCESS) {
        err("send_message error ", ret);
    }
}

void start_point(void **stateptr, __attribute__((unused)) size_t nbytes, void *data) {
    *stateptr = malloc(sizeof(state_t));
    ans_t *ans = (ans_t*)data;
    if (ans->n == ans->k) {
        printf("%lld\n", ans->k_fact);
        fflush(stdout);
        if (ans->parent != actor_id_self()) {
            message_t message = get_message(MSG_GODIE, 0, NULL);
            int ret = send_message(ans->parent, message);
            if (ret != SUCCESS) {
                err("last actor couldn't kill neighbor ", ret);
            }
        }
        message_t message = get_message(MSG_GODIE, 0, NULL);
        int ret = send_message(actor_id_self(), message);
        if (ret != SUCCESS) {
            err("last actor couldn't suicide ", ret);
        }
        free(ans);
        free(*stateptr);
        return;
    }
    if (ans->k != 0) {
        message_t message = get_message(MSG_GODIE, 0, NULL);
        int ret = send_message(ans->parent, message);
        if (ret != SUCCESS) {
            fprintf(stderr, "actor = %lu\n", actor_id_self());
            err("couldn't kill parent ", ret);
        }
    }
    actor_id_t actor = actor_id_self();
    state_t *my_state = *stateptr;
    my_state->ans = ans;
    ans->k_fact = ans->k_fact * (ans->k + 1);
    ans->k = ans->k + 1;
    ans->n = ans->n;
    message_t message = get_message(MSG_SPAWN, sizeof(role_t), factorial_role);
    int ret = send_message(actor, message);
    if (ret != SUCCESS) {
        err("send message error spawn ", ret);
    }
}

void factorial() {
    actor_id_t root;
    int ret = actor_system_create(&root, root_role);
    if (ret != SUCCESS) {
        err("system create error ", ret);
    }
    actor_system_join(root);
}

int main(){
    scanf("%d", &n);
    act_t acts_child[] = {hello, start_point, wait_child};
    act_t acts_root[] = {hello_root, start_point, wait_child};
    role_t *role_child = malloc(sizeof(role_t));
    role_child->nprompts = NPROMTS;
    role_child->prompts = acts_child;
    factorial_role = role_child;
    role_t *role_root = malloc(sizeof(role_t));
    role_root->nprompts = NPROMTS;
    role_root->prompts = acts_root;
    root_role = role_root;
    factorial();
    free(role_child);
    free(root_role);
	return 0;
}