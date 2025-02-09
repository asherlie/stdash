#include <stdatomic.h>

struct node{
    struct node* next;
    void* data;
};

struct ll{
    _Atomic (struct node*) first;
    _Atomic (struct node*) last;
};

void insert_ll(struct ll* l, void* data);
void p_ll(struct ll* l);
