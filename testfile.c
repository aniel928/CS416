#include "my_pthread_t.h"
#include <time.h>

void testThreads(void* arg){
	int i = 0;
	while(i<10000000){
		i++;
		
	}
	printf("thread%d\n", *((int*)arg));
	return;
}

void testThreadsWithExit(void* arg){
	int i = 0;
	while(i<100000000){
		i++;
	}
	printf("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
	my_pthread_exit(NULL);
	return;
}

int main(int argc, char** argv){
struct timespec start, end;
clock_gettime(CLOCK_MONOTONIC_RAW, &start);

my_pthread_t mythreads[100] = {0};
int i = 0;
while(i < 100){
	my_pthread_create(&mythreads[i], NULL, (void*)&testThreadsWithExit, NULL);
	printf("Thread %d created\n", i);
	i++;
}
i =0;
while(i< 100){
	my_pthread_join(mythreads[i], NULL);
	i++;
}

clock_gettime(CLOCK_MONOTONIC_RAW, &end);
uint delta_us = (end.tv_sec - start.tv_sec)*1000000 +(end.tv_nsec - start.tv_nsec) / 1000;

printf("%d\n", delta_us);

//my_pthread_join(mythreads[5 -1], NULL);
/*my_pthread_yield(mythread1);*/



}