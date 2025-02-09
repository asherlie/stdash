#include <pthread.h>

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
	struct thread_entry* next, * prev;
};

struct thread_ll{
    /*
     * only used in insert_thread(), remove_thread(), print
     * this is relevant for expand_pool
    */
	pthread_mutex_t lock;
	int sz;
	struct thread_entry* first, * last;
};

struct pool{
	struct routine_queue* routines;
	struct thread_ll* running, * ready;

	_Atomic int size_shift;
	_Atomic int assign_id;
	_Atomic int total_threads;
};

void init_pool(struct pool* p, int n);
void exec_routine(struct pool* p, void *(*routine)(void*), void* arg);
void shrink_pool(struct pool* p, int by);
void expand_pool(struct pool* p, int by);
void p_thread_ll(struct thread_ll* threads);
