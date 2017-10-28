#include "my_pthread_t.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
//#include <pthread.h>
#define NUM 30
pthread_mutex_t* mutex1 = NULL;

 
 void testThreadsWithExit(void* arg){
//	pthread_mutex_lock(mutex1);
	int j = 0;
	while(j<1000000000){
		j++;
	}
//	pthread_mutex_unlock(mutex1);

	printf("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
	pthread_exit(NULL);
//	pthread_t mythread2 = NULL;
//	pthread_create(&mythread2, NULL,(void*)&printMe, NULL);
// 	pthread_join(mythread2, NULL); 
 	return;
}



void returnNumber(){
	int a = 4;
	printf("YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY\n");
	pthread_exit((void*)&a);
}
/*

void readFromFile(){

	
    FILE *fptr;
    char*  filename = "./loremIpsum.txt";
    char ch;
 
    //  open the file for reading 
    fptr = fopen(filename, "r");
    if (fptr == NULL)
    {
        printf("Cannot open file \n");
        exit(0);
    }
    ch = fgetc(fptr);
    while (ch != EOF)
    {
        printf ("%c", ch);
        ch = fgetc(fptr);
    }
    fclose(fptr);

}
*/


int main(int argc, char** argv){
 	struct timespec start, end;
 	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
 	
// 	pthread_mutex_init(mutex1, NULL);
 	
 	pthread_t mythreads[NUM] = {0};
 	int i = 0;
 	while(i < NUM){
 		pthread_create(&mythreads[i], NULL, (void*)&testThreadsWithExit, NULL);
 		printf("Thread %d created\n", i);
		pthread_create(&mythreads[i+1], NULL, (void*)&returnNumber, NULL);
 		printf("Thread %d created\n", i+1);
		
 		i+=2;
 	}
 	i =0;
 	while(i< NUM){
 		pthread_join(mythreads[i], NULL);
 		i++;
 	}
// 	pthread_mutex_destroy(mutex1);
 	clock_gettime(CLOCK_MONOTONIC_RAW, &end);
 	uint delta_us = (end.tv_sec - start.tv_sec)*1000000 +(end.tv_nsec - start.tv_nsec) / 1000;
 
	printf("%d\n", delta_us);
	
	
/*
	pthread_yield(mythread1);

	pthread_t mythread = 0;
	
	pthread_create(&mythread, NULL, (void*)returnNumber, NULL);
	
	void* ptr = NULL;
	
	pthread_join(mythread, &ptr);
	
	int* intPtr = (int*)ptr;

	printf("%d\n",*intPtr);
	printf("tcb %d\n", sizeof(tcb));
	printf("queue %d\n", sizeof(queueNode));
	printf("next %d\n", sizeof(nextId));
*/
	

}

/*
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "pthread_t.h"
#include <sys/time.h>
#include <signal.h>

void testThreads(void* arg){
	int i = 0;
	beginTimer();
	printf("thread%d\n", *((int*)arg));
	return;
}

void testThreadsWithExit(){
	printf("Printing\n");
	beginTimer();
	pthread_exit(NULL);
	return;
}
int j;
void timer_handler (int signum)
{
 static int count = 0;
  ++count;
 if ((count%30) == 0){
	printf ("Test 3 (total time 3 seconds), currently run: %.3f seconds\n", count*.025);
 }
 j++;
}
beginTimer(){
	struct sigaction sa;
	struct itimerval timer;
	
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &timer_handler;
	sigaction (SIGVTALRM, &sa, NULL);

 
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 25000;	//25 ms
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 25000; //25 ms
	setitimer (ITIMER_VIRTUAL, &timer, NULL);

 
 while (j<120){}
	
}


int main(int argc, char** argv){

pthread_t mythread1, mythread2, mythread3, mythread4, mythread5, mythread6, mythread7, mythread8, mythread9, mythread10 = 0;
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
int *eights = &eight;
int nine = 9;
int *nines = &nine;
int ten = 10;
int *tens = &ten;




pthread_create(&mythread1, NULL, (void*)&testThreads, (void*)ones);
printf("%d\n",mythread1);
pthread_create(&mythread2, NULL, (void*)&testThreads, (void*)twos);
printf("%d\n",mythread2);
pthread_create(&mythread3, NULL, (void*)&testThreads, (void*)threes);
printf("%d\n",mythread3);
pthread_create(&mythread4, NULL, (void*)&testThreads, (void*)fours);
printf("%d\n",mythread4);
pthread_create(&mythread5, NULL, (void*)&testThreads, (void*)fives);
printf("%d\n",mythread5);
pthread_create(&mythread6, NULL, (void*)&testThreads, (void*)sixs);
printf("%d\n",mythread6);
pthread_create(&mythread7, NULL, (void*)&testThreads, (void*)sevens);
printf("%d\n",mythread7);
pthread_create(&mythread8, NULL, (void*)&testThreads, (void*)eights);
printf("%d\n",mythread8);
pthread_create(&mythread9, NULL, (void*)&testThreads, (void*)nines);
printf("%d\n",mythread9);
pthread_create(&mythread10, NULL, (void*)&testThreads, (void*)tens);
printf("%d\n",mythread10);


pthread_join(mythread1, NULL);
pthread_join(mythread2, NULL);
pthread_join(mythread3, NULL);
pthread_join(mythread4, NULL);
pthread_join(mythread5, NULL);
pthread_join(mythread6, NULL);
pthread_join(mythread7, NULL);
pthread_join(mythread8, NULL);


}


*/