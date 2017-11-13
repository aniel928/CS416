#include "my_pthread_t.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
//#include <pthread.h>
// #define NUM 30
// pthread_mutex_t* mutex1 = NULL;

 
// 
// void returnNumber(){
// 	int a = 4;
// 	printf("YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY\n");
// 	char* ptr = (char*)malloc(5000);
// 	free(ptr);
// 	pthread_exit((void*)&a);
// }

void giveMeMem(){
	printf("mallocing 5000 bytes\n");
	char* ptr[1000];
	int i = 0;
	while(i < 1000){
		printf("request # %d\n", i);
		ptr[i] = (char*)malloc(5000);
		i++;
	}
	printf("malloced pointer: %p\n",ptr[0]);
//	showData();
	i = 0;
	while(i < 1000){
		free(ptr[i]);
		i++;
	}
//	printf("done freeing\n");

	pthread_exit(NULL);
	return;
}



int main(int argc, char** argv){
//	printf("Can i have 5 bytes?\n");
	//void* myptr = malloc(5);//needs to be malloc call
	//void* myptr2 = malloc(5);//needs to be malloc call
//	printf("thanks!\n");
	//free(myptr2);
//	printf("not sure if it worked though\n");
	//malloc(7);	
	pthread_t mythread[30];
//	printf("Creating\n");
	int i=0;
	while(i< 30){
		pthread_create(&mythread[i], NULL, (void*)&giveMeMem, NULL);
		i++;		
	}
	//pthread_create(&mythread, NULL, (void*)&giveMeMem, NULL);
//	printf("Joining\n");
	i=0;
	while(i< 30){
		pthread_join(mythread[i], NULL);
		i++;		
	}
	
//	showData();
}
