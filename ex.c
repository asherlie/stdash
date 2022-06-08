#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "tp.h"

void* test_routine(void* vn){
	int* n = vn;
	int sleep = 1;
	printf("%i\n", *n);
	usleep(sleep*1000000);
	return NULL;
}

int main(){
	struct pool p;
	int* n;
	init_pool(&p, 2);
	/*
	 * puts("running");
	 * p_thread_ll(p.running);
	 * puts("ready");
	 * p_thread_ll(p.ready);
	*/
    shrink_pool(&p, 1);
    expand_pool(&p, 10);
    for(int i = 0; i < 50; ++i){
		n = malloc(sizeof(int));
		*n = i;
		exec_routine(&p, test_routine, n);
	}
    shrink_pool(&p, 10);
	usleep(5000000);
    printf("finished with %i threads\n", p.total_threads);
    p_thread_ll(p.ready);
	return 0;
}
