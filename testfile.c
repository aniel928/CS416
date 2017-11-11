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
	printf("mallocing 500 bytes\n");
	char* ptr = (char*)malloc(500);
	free(ptr);
	pthread_exit(NULL);
}



int main(int argc, char** argv){
//	printf("Can i have 5 bytes?\n");
	//void* myptr = malloc(5);//needs to be malloc call
	//void* myptr2 = malloc(5);//needs to be malloc call
//	printf("thanks!\n");
	//free(myptr2);
//	printf("not sure if it worked though\n");
	//malloc(7);	
	pthread_t mythread[100];
//	printf("Creating\n");
	int i=0;
	while(i< 100){
		pthread_create(&mythread[i], NULL, (void*)&giveMeMem, NULL);
		i++;		
	}
	//pthread_create(&mythread, NULL, (void*)&giveMeMem, NULL);
//	printf("Joining\n");
	pthread_join(mythread[0], NULL);
}
