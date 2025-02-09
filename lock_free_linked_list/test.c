#include "lf_ll.h"

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
/*
 * write a test that uses n threads to insert n values
 * then check if each number occurs the right nubmer of times
 * ex. 4 threads with 100 values
 * int check_arr[10] = {100, 100, 100, 100};
 * iterate over, --check_arr[thread_no] each time we find a number!
*/

struct ins_arg{
    int n_threads;
    int ins_per_thread;
    int thread_id;
    int sleep_us_bound;

    struct ll* l;
};


void* insertion_thread(void* varg) {
    struct ins_arg* arg = varg;
    for (int i = 0; i < arg->ins_per_thread; ++i) {
        if (arg->sleep_us_bound) {
            /*usleep(sleep_us_bound);*/
            usleep(random() % arg->sleep_us_bound);

        }
        insert_ll(arg->l, (void*)(uint64_t)arg->thread_id);
    }

    free(varg);
    return NULL;
}

_Bool concurrent_test(int n_threads, int ins_per_thread, int max_sleep, _Bool debug) {
    struct ll l = {0};
    pthread_t th[n_threads];
    struct ins_arg* arg;
    int expected_data[n_threads];

    for (int i = 0; i < n_threads; ++i) {
        expected_data[i] = ins_per_thread;
        arg = malloc(sizeof(struct ins_arg));
        arg->sleep_us_bound = max_sleep;
        arg->l = &l;
        arg->thread_id = i;
        arg->n_threads = n_threads;
        arg->ins_per_thread = ins_per_thread;
        pthread_create(th + i, NULL, insertion_thread, arg);
        if (debug) {
            printf("thread %i created\n", i);
        }
    }

    for (int i = 0; i < n_threads; ++i) {
        pthread_join(th[i], NULL);

        if (debug) {
            printf("thread %i joined\n", i);
        }
    }

    for (struct node* n = l.first; n; n = n->next) {
        --expected_data[(uint64_t)n->data];
        if (debug) {
            printf("decremented expected_data[%li] -> %i\n", (uint64_t)n->data, expected_data[(uint64_t)n->data]);
        }
    }

    for (int i = 0; i < n_threads; ++i) {
        if (expected_data[i]) {
            printf("FAILURE: found nonzero value at expected_data[%i]\n", i);
            return 0;
        }
    }
    /*p_ll(&l);*/
    return 1;
}

int main(){
    struct ll l;
    insert_ll(&l, (void*)1);
    insert_ll(&l, (void*)1);
    for (uint64_t i = 0; i < 13; ++i) {
        insert_ll(&l, (void*)i);
    }

    p_ll(&l);

    if (concurrent_test(50, 500001, 0, 0)) {
        puts("Many insertion SUCCESS! linked list is exactly as expected");
    }

    if (concurrent_test(50, 500, 1000, 0)) {
        puts("Random sleep SUCCESS! linked list is exactly as expected");
    }

    return 1;
}
