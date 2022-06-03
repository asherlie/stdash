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

struct routine{
	void *(*routine)(void *);
	void* arg;
};

struct routine_entry{
	struct routine* routine;
	struct routine_entry* next;
};

struct routine_queue{
	pthread_mutex_t lock;
	pthread_cond_t nonempty;
	_Atomic int n;
	struct routine_entry* first, * last;
};

struct thread{
	pthread_t pth;
	int id;
	char* name;

	/* each thread has a pointer to pool in order to access stats like available/running threads
	 * and the routine queue
	 */
	struct pool* pool_backref;
};

struct thread_entry{
	struct thread* thread;
	struct thread_entry* next;
};

struct thread_ll{
	pthread_mutex_t lock;
	int sz;
	struct thread_entry* first, * last;
};

struct pool{
	struct routine_queue* routines;
	struct thread_ll* running, * ready;

	_Atomic int waiting_threads;
};



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
		threads->first = threads->last = e;
	}
	else{
		threads->last->next = e;
		threads->last = e;
	}
	++threads->sz;
	pthread_mutex_unlock(&threads->lock);

	return e->thread;
}

// moves from running to ready / vice versa
void swap_thread_entry(){
}

// threads should be moved from ready -> running
// but code should work without this
void* wait_and_exec(void* vt){
	struct thread* t = vt;
	struct routine* r;
	for(;;){	
		r = pop_rq(t->pool_backref->routines);
		r->routine(r->arg);
		/*pthread_cond_*/
	}
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
*/
void exec_routine(struct pool* p, void *(*routine)(void*), void* arg){
	insert_routine_queue(p->routines, routine, arg);
}

void p_thread(struct thread* t, int spaces){
	for(int i = 0; i < spaces; ++i)printf(" ");
	printf("id: %i, name: \"%s\"\n", t->id, t->name ? t->name : "");
}

int set_thread_number(struct pool* p, int n){
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

void* test_routine(void* vn){
	int* n = vn;
	int sleep = 5;
	printf("%i\n", *n);
	usleep(sleep*1000000);
	return NULL;
}

int main(){
	struct pool p;
	int* n;
	init_pool(&p, 10);
	/*
	 * puts("running");
	 * p_thread_ll(p.running);
	 * puts("ready");
	 * p_thread_ll(p.ready);
	*/
	for(int i = 0; i < 50; ++i){
		n = malloc(sizeof(int));
		*n = i;
		exec_routine(&p, test_routine, n);
	}
	usleep(10000000);
	return 0;
}
