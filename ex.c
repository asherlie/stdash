#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "tp.h"

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
    shrink_pool(&p, 4);
    for(int i = 0; i < 50; ++i){
		n = malloc(sizeof(int));
		*n = i;
		exec_routine(&p, test_routine, n);
	}
	usleep(10000000);
	return 0;
}
