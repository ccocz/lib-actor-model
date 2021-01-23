#include "minunit.h"
#include "cacti.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int tests_run = 0;

static char *empty()
{
	mu_assert("empty", true);
	return 0;
}

static char *initial() {
    actor_id_t root;
    role_t *root_role = NULL;
    int ret = actor_system_create(&root, root_role);
    mu_assert("root created", ret == SUCCESS);
    mu_assert("root id", root == ROOT_ID);
    actor_system_join(root);
    return 0;
}

void my_hello(void **stateptr, size_t nbytes, void *data) {
    message_t message;
    message.data = NULL;
    message.nbytes = 0;
    message.message_type = MSG_GODIE;
    send_message(actor_id_self(), message);
    //actor_id_t id = (actor_id_t)data;
    //fprintf(stderr, "%lu says hello to %lu\n", id, actor_id_self());
    //fflush(stderr);
}

static char *hello() {
    actor_id_t root;
    role_t *root_role = malloc(sizeof(role_t));
    root_role->nprompts = 1;
    act_t acts[] = {my_hello};
    root_role->prompts = acts;
    int ret = actor_system_create(&root, root_role);
    mu_assert("root created", ret == SUCCESS);
    mu_assert("root id", root == ROOT_ID);
    message_t message;
    actor_id_t id = 3;
    message.data = (void*)id;
    message.nbytes = sizeof(actor_id_t);
    message.message_type = MSG_HELLO;
    send_message(root, message);
    actor_system_join(root);
    actor_system_join(root);
    actor_system_join(root);
    free(root_role);
    return 0;
}

static char *spawn() {
    actor_id_t root;
    role_t *root_role = NULL;
    int ret = actor_system_create(&root, root_role);
    mu_assert("root created", ret == SUCCESS);
    mu_assert("root id", root == ROOT_ID);
    role_t *child_role = malloc(sizeof(role_t));
    child_role->nprompts = 1;
    act_t acts[] = {my_hello};
    child_role->prompts = acts;
    message_t message;
    message.data = (void*)child_role;
    message.nbytes = sizeof(role_t);
    message.message_type = MSG_SPAWN;
    send_message(root, message);
    send_message(root, message);
    send_message(root, message);
    send_message(root, message);
    send_message(root, message);
    send_message(root, message);
    send_message(root, message);
    actor_system_join(root);
    free(child_role);
    return 0;
}

static char *godie() {
    actor_id_t root;
    role_t *root_role = NULL;
    int ret = actor_system_create(&root, root_role);
    mu_assert("root created", ret == SUCCESS);
    mu_assert("root id", root == ROOT_ID);
    message_t message;
    message.data = NULL;
    message.nbytes = 0;
    message.message_type = MSG_GODIE;
    send_message(root, message);
    sleep(2);
    ret = send_message(root, message);
    mu_assert("must be dead", ret == ACTOR_NOT_ALIVE_ERR);
    actor_system_join(root);
    return 0;
}


static char *all_tests()
{
    //mu_run_test(initial);
    mu_run_test(hello);
    //mu_run_test(spawn);
    //while (1) {
        //mu_run_test(spawn);
        //fprintf(stderr, "============\n");
        //fflush(stderr);
    //}

    //mu_run_test(godie);
    return 0;
}

int main()
{
    char *result = all_tests();
    /*if (result != 0)
    {
        printf(__FILE__ ": %s\n", result);
    }
    else
    {
        printf(__FILE__ ": ALL TESTS PASSED\n");
    }
    printf(__FILE__ ": Tests run: %d\n", tests_run);*/

    return result != 0;
}

//orchestrates a pool of threads to drive all actions completely transparently