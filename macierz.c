#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cacti.h"
#include "error.h"

#define NRPROMPTS 5

#define MSG_WAKE_ROOT 1
#define MSG_INFORM_ACTOR 2
#define MSG_INFORM_ROOT 3
#define MSG_START_SUM_OPERATION 4
#define MSG_SUM_ONLY_ONE_COLUMN 5

#define TRUE 1
#define FALSE 0

#define MILLIS 1000

typedef struct cell {
    int value;
    int timeout;
} cell_t;

typedef struct actor_info {
    int column;
    int is_last;
    int cnt_done;
    actor_id_t neighbour;
    actor_id_t root;
} actor_info_t;

typedef struct root_info {
    int last_column;
    int cnt_ready;
    actor_id_t last_actor;
} root_info_t;

typedef struct ans {
    int row;
    int sum;
} ans_t;

int rows;
int columns;
cell_t **grid;
int *row_sum;

static message_t get_message(message_type_t type, size_t nbytes, void* data) {
    message_t message;
    message.message_type = type;
    message.nbytes = nbytes;
    message.data = data;
    return message;
}

void hello(void **stateptr, size_t nbytes, void *data) {
    (void)stateptr;
    (void)nbytes;
    actor_id_t my_id = actor_id_self();
    actor_id_t parent_id = (actor_id_t)data;
    message_t message = get_message(MSG_WAKE_ROOT, sizeof(actor_id_t), (void *) my_id);
    int ret = send_message(parent_id, message);
    if (ret != SUCCESS) {
        err("send message error ", ret);
    }
}

void gather_info(void **stateptr, size_t nbytes, void *data) {
    (void)nbytes;
    actor_info_t *actor_info = (actor_info_t*)data;
    *stateptr = actor_info;
    message_t message = get_message(MSG_INFORM_ROOT, 0, NULL);
    int ret = send_message(actor_info->root, message);
    if (ret != SUCCESS) {
        err("send_message error ", ret);
    }
}

void wait_columnar(void **stateptr, size_t nbytes, void *data) {
    (void)nbytes;
    root_info_t *root_info;
    if (*stateptr == NULL) {
        *stateptr = malloc(sizeof(root_info_t));
        root_info = *stateptr;
        root_info->last_column = columns - 1;
        root_info->last_actor = actor_id_self();
        root_info->cnt_ready = 0;
    } else {
        root_info = *stateptr;
    }
    actor_id_t send_to = (actor_id_t)data;
    actor_info_t *actor_info = malloc(sizeof(actor_info_t));
    actor_info->column = root_info->last_column;
    actor_info->is_last = root_info->last_column == columns - 1 ? TRUE : FALSE;
    actor_info->neighbour = root_info->last_actor;
    actor_info->root = actor_id_self();
    actor_info->cnt_done = 0;
    root_info->last_column--;
    root_info->last_actor = send_to;
    message_t message = get_message(MSG_INFORM_ACTOR, sizeof(actor_info_t), actor_info);
    int ret = send_message(send_to, message);
    if (ret != SUCCESS) {
        err("send_message error ", ret);
    }
}

void wait_to_start(void **stateptr, size_t nbytes, void *data) {
    (void)nbytes;
    (void)data;
    root_info_t *root_info = *stateptr;
    root_info->cnt_ready++;
    if (root_info->cnt_ready == columns - 1) {
        actor_id_t neighbor = root_info->last_actor;
        free(root_info);
        actor_info_t *actor_info = malloc(sizeof(actor_info_t));
        actor_info->root = actor_id_self();
        actor_info->neighbour = neighbor;
        actor_info->is_last = FALSE; // todo
        actor_info->column = 0;
        actor_info->cnt_done = 0;
        *stateptr = actor_info;
        for (int i = 0; i < rows; i++) {
            ans_t *ans = malloc(sizeof(ans_t));
            ans->row = i;
            ans->sum = 0;
            message_t message = get_message(MSG_START_SUM_OPERATION, sizeof(ans_t), ans);
            int ret = send_message(actor_info->root, message);
            if (ret != SUCCESS) {
                err("couldn't send start row sum communication ", ret);
            }
        }
    }
}

void sum_cell(void **stateptr, size_t nbytes, void *data) {
    (void)nbytes;
    actor_info_t *actor_info = *stateptr;
    ans_t *ans = (ans_t*)data;
    int row = ans->row;
    int column = actor_info->column;
    usleep(MILLIS * grid[row][column].timeout);
    ans->sum += grid[row][column].value;
    if (actor_info->is_last) {
        row_sum[ans->row] = ans->sum;
        free(ans);
    } else {
        message_t message = get_message(MSG_START_SUM_OPERATION, sizeof(ans_t), ans);
        int ret = send_message(actor_info->neighbour, message);
        if (ret != SUCCESS) {
            err("actor couldn't send sum to neighbor ", ret);
        }
    }
    actor_info->cnt_done++;
    if (actor_info->cnt_done == rows) {
        free(actor_info);
    }
}

void sum_all(void **stateptr, size_t nbytes, void *data) {
    (void)stateptr;
    (void)nbytes;
    (void)data;
    for (int i = 0; i < rows; i++) {
        usleep(MILLIS * grid[i][0].timeout);
        row_sum[i] = grid[i][0].value;
    }
}

void rows_sum(role_t *role) {
    actor_id_t root;
    int ret = actor_system_create(&root, role);
    if (ret != SUCCESS) {
        err("error system create ", ret);
    }
    message_t message = get_message(MSG_SPAWN, sizeof(role_t), role);
    for (int i = 0; i < columns - 1; i++) { // -1 for root
        ret = send_message(root, message);
        if (ret != SUCCESS) {
            err("root couldn't create actor ", ret);
        }
    }
    if (columns == 1) {
        message = get_message(MSG_SUM_ONLY_ONE_COLUMN, 0, NULL);
        ret = send_message(root, message);
        if (ret != SUCCESS) {
            err("root couldn't send message to himself ", ret);
        }
    }
    actor_system_join(root);
}

int main(){
    scanf("%d %d", &rows, &columns);
    row_sum = malloc(sizeof(int) * rows); // todo ll
    grid = malloc(rows * sizeof(cell_t*));
    for (int i = 0; i < rows; i++) {
        grid[i] = malloc(sizeof(cell_t) * columns);
    }
    cell_t next;
    for (int i = 0; i < rows * columns; i++) {
        scanf("%d %d", &next.value, &next.timeout);
        grid[i / columns][i % columns] = next;
    }
    act_t acts[] = {hello, wait_columnar, gather_info, wait_to_start, sum_cell, sum_all};
    role_t *role = malloc(sizeof(role_t));
    role->nprompts = NRPROMPTS;
    role->prompts = acts;
    rows_sum(role);
    for (int i = 0; i < rows; i++) {
        printf("%d\n", row_sum[i]);
        free(grid[i]);
    }
    free(grid);
    free(row_sum);
    free(role);
	return 0;
}