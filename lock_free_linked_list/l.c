#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>

// TODO: define a struct with a circular


/*
 * lock free strat:
 *     if firstr is null, swap out the WHOLE thing...obviously - if we need to CAS two things just go a layer more abstracted
 *
 *     OR, since when first == NULL, last will never be updated, we can just take our time
*/
struct node{
    struct node* next;
    void* data;
};

struct ll{
    _Atomic (struct node*) first;
    _Atomic (struct node*) last;
};

void insert_ll(struct ll* l, void* data) {
    struct node* n = malloc(sizeof(struct node));
    _Atomic (struct node*) nil_n = NULL;
    n->data = data;
    n->next = NULL;
    /* if !first, update it */
    if (atomic_compare_exchange_strong(&l->first, &nil_n, n)) {
        /*
         * if the above has succeeded, no other threads will succeed
         * and no thread will insert after last because last is NULL
        */
        atomic_store(&l->last, n);
        /*puts("succesful CAS() for idx 0");*/
        return;
    }
    while (1) {
        nil_n = l->last;
        /* set last to new node */
        if (atomic_compare_exchange_strong(&l->last, &nil_n, n)) {
            /* i don't believe that this needs to be atomic */
            nil_n->next = n;
            return;
        }
    }
}

void p_ll(struct ll* l) {
    for (_Atomic (struct node*) n = l->first; n; n = n->next) {
        printf("%li -> ", (uint64_t)n->data);
    }
    puts("\\0");
}

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

    struct ll* l;
};


void* insertion_thread(void* varg) {
    struct ins_arg* arg = varg;
    for (int i = 0; i < arg->ins_per_thread; ++i) {
        insert_ll(arg->l, (void*)(uint64_t)arg->thread_id);
    }

    free(varg);
    return NULL;
}

_Bool concurrent_test(int n_threads, int ins_per_thread, _Bool debug) {
    struct ll l = {0};
    pthread_t th[n_threads];
    struct ins_arg* arg;
    int expected_data[n_threads];

    for (int i = 0; i < n_threads; ++i) {
        expected_data[i] = ins_per_thread;
        arg = malloc(sizeof(struct ins_arg));
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

    if (concurrent_test(50, 50001, 0)) {
        puts("SUCCESS! linked list is exactly as expected");
    }

    return 1;
}
