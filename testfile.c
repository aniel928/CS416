#include "my_pthread_t.h"

void testThreads(void* arg){
	int i = 0;
	while(i<1000000000){
		i++;
		
	}
	printf("thread%d\n", *((int*)arg));
	return;
}

void testThreadsWithExit(void* arg){
	int i = 0;
	while(i<1000000000){
		i++;
		
	}
	printf("thread%d\n", *((int*)arg));
	my_pthread_exit(NULL);
	return;
}

int main(int argc, char** argv){

my_pthread_t mythread1, mythread2, mythread3, mythread4, mythread5, mythread6, mythread7, mythread8, mythread9, mythread10, mythread11, mythread12 = 0;
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
int seven = 7;
int* sevens = &seven;
int eight = 8;
int* eights = &eight;
int nine = 9;
int* nines = &nine;
int ten = 10;
int* tens = &ten;
int eleven = 11;
int* elevens = &eleven;
int twelve = 12;
int* twelves = &twelve;

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
my_pthread_create(&mythread7, NULL, (void*)&testThreadsWithExit, (void*)sixs);
printf("Thread # %d!\n",mythread7);
my_pthread_create(&mythread8, NULL, (void*)&testThreadsWithExit, (void*)sixs);
printf("Thread # %d!\n",mythread8);
my_pthread_create(&mythread9, NULL, (void*)&testThreadsWithExit, (void*)sixs);
printf("Thread # %d!\n",mythread9);
my_pthread_create(&mythread10, NULL, (void*)&testThreadsWithExit, (void*)sixs);
printf("Thread # %d!\n",mythread10);
my_pthread_create(&mythread11, NULL, (void*)&testThreadsWithExit, (void*)sixs);
printf("Thread # %d!\n",mythread11);
my_pthread_create(&mythread12, NULL, (void*)&testThreadsWithExit, (void*)sixs);
printf("Thread # %d!\n",mythread12);
my_pthread_join(mythread1, NULL);
printf("please don't print this after joining thread 1.\n");
my_pthread_join(mythread2, NULL);
my_pthread_join(mythread3, NULL);
my_pthread_join(mythread4, NULL);
}