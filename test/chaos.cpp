#include <cstdlib>
#include <atomic>
#include <random>

extern "C" {
#include "../cacti.h"
}

#define AMPLIFICATION_FACTOR 100

std::mt19937 init_random() {
    std::random_device rd;
    return std::mt19937(rd());
}

static std::atomic<int> cnt;

static std::atomic_long max_actor_id;
static thread_local std::mt19937 t_random = init_random();
static std::atomic_long rx_msgs;
static int rx_one_times = 0;

static void do_chaos(void**, size_t, void*);

static act_t f1[1] = {do_chaos};
static role_t r1 = {1, f1};

const std::string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}

static void do_chaos(void**, size_t, void*) {
    if (actor_id_self() == 0) {
        //printf("[%li] do_chaos: %li\n", actor_id_self(), rx_msgs.load());
        if (++rx_one_times == 10) {
            // (try to) kill all actors
            for (int i = 0; i <= max_actor_id; i++)
                send_message(i, {MSG_GODIE, 0, NULL});
            rx_one_times = 0;
        }
        rx_msgs = 0;
        //printf("finished, cnt_actor = %ld, %s\n", max_actor_id.load(), &currentDateTime()[0]);
    }
    rx_msgs.fetch_add(1);
    auto cur_max_actor_id = max_actor_id.load();
    while (actor_id_self() > cur_max_actor_id)
        max_actor_id.compare_exchange_weak(cur_max_actor_id, actor_id_self());

    for (int i = 0; i < AMPLIFICATION_FACTOR; i++) {
        std::uniform_int_distribution<int> distribution(0, max_actor_id + 5);
        auto target = distribution(t_random);

        std::uniform_int_distribution<int> distribution2(0, 4);
        message_type_t msg_types[] = {MSG_HELLO, MSG_HELLO, MSG_HELLO, MSG_GODIE, MSG_SPAWN};
        auto msg_type = msg_types[distribution2(t_random)];

        if ((msg_type == MSG_GODIE && target == 0) || (msg_type == MSG_SPAWN && cur_max_actor_id > 100)) // protect the first actor against death
            continue;
        //printf("CNT = %d, before target = %d, msg_type = %ldl, i = %d\n", cnt++, target, msg_type, i);
        int stat = send_message(target, {msg_type, static_cast<size_t>(cnt - 1), &r1});
        //printf("CNT = %d, after target = %d, msg_type = %ldl, status = %d, i = %d\n", cnt++, target, msg_type, stat, i);
        //fflush(stdout);
    }

    send_message(actor_id_self(), {MSG_HELLO, 0, &r1});
}


int main() {
    actor_id_t initial;
    for (long int i = 0; i < 1; i++) {
        printf("Loop #%li\n", i);

        max_actor_id = 0;
        rx_one_times = 0;
        rx_msgs = 0;

        int ret = actor_system_create(&initial, &r1);
        if (ret < 0)
            abort();
        actor_system_join(initial);

        printf("-> Complete! Max actor id: %li\n", max_actor_id.load());
    }
}