#if 0
/*

threads each have an identifier and a group - identifiers are guaranteed to be unique
group is not - group is set by the user and when a routine is scheduled to be run, a group can be
specified

this can be used to prioritize certain tasks by providing more resources for it
hmm, this could be implemented by just using two pools - might be simpler

TODO: need to be able to adjust number of threads

TODO: need to wait until n routines have executed before joining
TODO: need to wait until all running routines have executed before joining
*/
#endif
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>

#include "tp.h"

void init_routine_queue(struct routine_queue* rq){
	pthread_mutex_init(&rq->lock, NULL);
	pthread_cond_init(&rq->nonempty, NULL);
	rq->n = 0;
	rq->first = rq->last = NULL;
}

void insert_routine_queue(struct routine_queue* rq, void *(*routine)(void *), void* arg){
	struct routine_entry* re = malloc(sizeof(struct routine_entry));
	re->routine = malloc(sizeof(struct routine));
	re->next = NULL;
	re->routine->routine = routine;
	re->routine->arg = arg;

	pthread_mutex_lock(&rq->lock);
	if(!rq->first){
		rq->first = rq->last = re;
	}
	else{
		/*printf("set last->next to %p\n", (void*)routine);*/
		rq->last->next = re;
		rq->last = re;
	}
	atomic_fetch_add(&rq->n, 1);
	pthread_cond_signal(&rq->nonempty);
	pthread_mutex_unlock(&rq->lock);
}

struct routine* pop_rq(struct routine_queue* rq){
	struct routine* ret;
	/*while(!atomic_load(&rq->n)){*/
	for(;;){
		pthread_mutex_lock(&rq->lock);
		/* no need for atomic_load() here */
		/*if(atomic_load(&rq->n))*/
		if(!rq->n){
			pthread_mutex_unlock(&rq->lock);
			pthread_cond_wait(&rq->nonempty, &rq->lock);
		}
		/* potential bad cond signals */
		if(!rq->first)pthread_mutex_unlock(&rq->lock);
		else break;
	}
	// rq is nonempty and we have the lock
	ret = rq->first->routine;
	rq->first = rq->first->next;
	atomic_fetch_add(&rq->n, -1);
	pthread_mutex_unlock(&rq->lock);
	return ret;
}

void init_thread_ll(struct thread_ll* threads){
	pthread_mutex_init(&threads->lock, NULL);
	threads->sz = 0;
	threads->first = threads->last = NULL;
}

/*struct thread* insert_thread_ll(struct thread_ll* threads, pthread_t pth, int id, char* name){*/
struct thread* insert_thread_ll(struct thread_ll* threads, struct thread* t){
	struct thread_entry* e = malloc(sizeof(struct thread_entry));
	e->next = NULL;
	e->thread = t;
	/*
	 * e->thread->pth = pth;
	 * e->thread->id = id;
	 * e->thread->name = name;
	*/

	pthread_mutex_lock(&threads->lock);
	if(!threads->first){
        e->prev = NULL;
		threads->first = threads->last = e;
	}
	else{
        e->prev = threads->last;
		e->prev->next = e;
		threads->last = e;
	}
	++threads->sz;
	pthread_mutex_unlock(&threads->lock);

	return e->thread;
}

// moves from running to ready / vice versa
void swap_thread_entry(){
}

_Bool time_to_exit(struct pool* p){
    int shift = atomic_load(&p->size_shift);
    if(shift >= 0)return 0;
    /* this is not guaranteed to exit when it should,
     * if two threads ought to exit at the same time, 
     * there's a chance that only one will exit
     */
    /* TODO: ABA problem? shrink, add, shrink */
    if(atomic_compare_exchange_strong(&p->size_shift, &shift, shift+1)){
        printf("decremented %i shrinks to %i\n", shift, shift+1);
        return 1;
    }
    return 0;
}

/* it's okay for this to not be efficient because 
 * it doesn't conflict with any other code - this is only
 * performed after wait_and_exec() is ready to shutdown
 * this can slow down the expanding of the pool
 *
 * TODO: just pass this the thread_entry that contains
 * prev, next
 */
void remove_thread_ll(struct pool* p, struct thread* t){
    struct thread_entry* e;
    pthread_mutex_lock(&p->ready->lock);
    for(e = p->ready->first; e; e = e->next){
        if(e->thread == t)break;
    }
    /*
     * rm:
     *     internal
     *         prev->next = next
     *         next->prev = prev
     *     first
     *         if(next)next->prev = NULL
     *         first = next
     *     last    
     *         if(prev)prev->next = NULL
     *         last = prev
    */
    /*if(!e)return;*/
    /* e is not the first element */
    /*what if e is last*/
    if(e->prev && e->next){
        e->prev->next = e->next;
        e->next->prev = e->prev;
    }
    else{
        // last
        if(e->prev){
            e->prev->next = NULL;
            p->ready->last = e->prev;
        }
        // first
        else{
            e->next->prev = NULL;
            p->ready->first = e->next;
        }
    }
    #if 0
    if(e->prev){
        e->prev->next = e->next;
        if(e->next)e->next->prev = e->prev;
        else p->ready->last = e;
    }
    else{
        p->ready->first = e->next;
        if(!p->ready->first)p->ready->last = NULL;
        /*if(p->ready->last == p->ready->first)*/
    }
    #endif
    atomic_fetch_add(&p->total_threads, -1);
    pthread_mutex_unlock(&p->ready->lock);
    /*free(t);*/
}

// threads should be moved from ready -> running
// but code should work without this
void* wait_and_exec(void* vt){
	struct thread* t = vt;
	struct routine* r;
	for(;;){	
		r = pop_rq(t->pool_backref->routines);
		if(r->routine)r->routine(r->arg);
        if(time_to_exit(t->pool_backref))break;
		/*pthread_cond_*/
	}
    printf("thread %i has exited!\n", t->id);
    remove_thread_ll(t->pool_backref, t);
    /*atomic_fetch_add(&t->pool_backref->total_threads, -1);*/

    return NULL;
}

struct thread* spawn_thread(int id, char* name, struct pool* p){
	struct thread* ret = malloc(sizeof(struct thread));
	ret->pool_backref = p;
	ret->id = id;
	ret->name = name;
	pthread_create(&ret->pth, NULL, wait_and_exec, ret);
	return ret;
}

void init_pool(struct pool* p, int n){
	p->running = malloc(sizeof(struct thread_ll));
	p->ready = malloc(sizeof(struct thread_ll));
	p->routines = malloc(sizeof(struct routine_queue));

    p->total_threads = n;
    p->size_shift = 0;

	/* this sets up a lock and cond that wait_and_exec() use */
	init_routine_queue(p->routines);

	init_thread_ll(p->running);
	init_thread_ll(p->ready);

	for(int i = 0; i < n; ++i){
		insert_thread_ll(p->ready, spawn_thread(i, NULL, p));
	}
}

/*
 * we don't really need these two separate thread lists do we...
 * well it doesn't add any overhead i suppose, because the lock isn't
 * related to the routine lock which each thread uses while its running
 * to find a routine
 * even when a thread is moved between lists it should be running
 *
 * it'll be helpful for resizing the pool - we can mark threads as ready to exit
 * upon shrinking
 *
 * or we can just atomically increment a 
*/
void exec_routine(struct pool* p, void *(*routine)(void*), void* arg){
	insert_routine_queue(p->routines, routine, arg);
}

void p_thread(struct thread* t, int spaces){
	for(int i = 0; i < spaces; ++i)printf(" ");
	printf("id: %i, name: \"%s\"\n", t->id, t->name ? t->name : "");
}

void shrink_pool(struct pool* p, int by){
    int nop = by-atomic_load(&p->routines->n);
    atomic_fetch_add(&p->size_shift, -by);
    /* queue enough NOPs to ensure that
     * all threads that need to exit will exit
     *
     * all running threads will exit after their
     * routines along with however many NOPs
     * were running to make up the difference
     */
    for(int i = 0; i < nop; ++i){
        exec_routine(p, NULL, NULL);
    }
}

/*
 * just need to increment size_shift, nay! no need
 * we can just insert spawned threads into the ready ll
 * and increment nthreads in pool
 * size_shift can still apply
 * in this case, we can make it positive bc it'll
 * never be neg
*/
void expand_pool(struct pool* p, int by){
    for(int i = 0; i < by; ++i)
        insert_thread_ll(p->ready, spawn_thread(-1, NULL, p));

    /* not a problem if this runs between thread exits,
     * the thread count will remain accurate
     */
    atomic_fetch_add(&p->total_threads, by);
}

int set_thread_number(struct pool* p, int n){
    (void)p;
    (void)n;
    return 0;
}

void p_thread_ll(struct thread_ll* threads){
	int idx = 0;
	pthread_mutex_lock(&threads->lock);
	for(struct thread_entry* e = threads->first; e; e = e->next){
		printf("printing index %i:\n", idx++);
		p_thread(e->thread, 2);
	}
	pthread_mutex_unlock(&threads->lock);
}
