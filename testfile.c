#include "my_pthread_t.h"

void testThreads(void* arg){
	int i = 0;
	while(i<100000000){
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
	printf("thread%d\n", *((int*)arg));
	my_pthread_exit(NULL);
	return;
}

int main(int argc, char** argv){

my_pthread_t mythread1, mythread2, mythread3, mythread4, mythread5, mythread6 = 0;
int one = 1;
int* ones = &one;
int two = 2;
int* twos = &two;
int three = 3;
int* threes = &three;
int four = 4;
int* fours = &four;
int five = 5;
int* fives = &five;
int six = 6;
int* sixs = &six;

my_pthread_create(&mythread1, NULL, (void*)&testThreadsWithExit, (void*)ones);
printf("Thread # %d!\n",mythread1);
my_pthread_create(&mythread2, NULL, (void*)&testThreadsWithExit, (void*)twos);
printf("Thread # %d!\n",mythread2);
my_pthread_create(&mythread3, NULL, (void*)&testThreadsWithExit, (void*)threes);
printf("Thread # %d!\n",mythread3);
my_pthread_create(&mythread4, NULL, (void*)&testThreadsWithExit, (void*)fours);
printf("Thread # %d!\n",mythread4);
my_pthread_create(&mythread5, NULL, (void*)&testThreadsWithExit, (void*)fives);
printf("Thread # %d!\n",mythread5);
my_pthread_create(&mythread6, NULL, (void*)&testThreadsWithExit, (void*)sixs);
printf("Thread # %d!\n",mythread6);
my_pthread_join(mythread1, NULL);
printf("please don't print this after joining thread 1.\n");
my_pthread_join(mythread2, NULL);
my_pthread_join(mythread3, NULL);
my_pthread_join(mythread4, NULL);
}