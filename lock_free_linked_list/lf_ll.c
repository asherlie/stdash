#include "lf_ll.h"

#include <unistd.h>
#include <string.h>
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
