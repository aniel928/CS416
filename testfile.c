#include "my_pthread_t.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>


my_pthread_mutex_t* mutex1 = NULL;

// 
// void testThreadsWithExit(void* arg){
// //	my_pthread_mutex_lock(mutex1);
// //	int j = 0;
// //	while(j<100000000000){
// //		j++;
// //	}
// //	printf("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
// //	my_pthread_exit(NULL);
// //	my_pthread_mutex_unlock(mutex1);
// 	my_pthread_t mythread2 = NULL;
// 	my_pthread_create(&mythread2, NULL,(void*)&printMe, NULL);
// 	my_pthread_join(mythread2, NULL); 
// 	return;
// }

void returnNumber(){
int a = 4;
my_pthread_exit((void*)&a);
}


void readFromFile(){

	
    FILE *fptr;
    char*  filename = "./loremIpsum.txt";
    char ch;
 
    /*  open the file for reading */
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



int main(int argc, char** argv){
// 	struct timespec start, end;
// 	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
// 	
// 	my_pthread_mutex_init(mutex1, NULL);
// 	
// 	my_pthread_t mythreads[20] = {0};
// 	int i = 0;
// 	while(i < 20){
// 		my_pthread_create(&mythreads[i], NULL, (void*)&readFromFile, NULL);
// 		printf("Thread %d created\n", i);
// 		i++;
// 	}
// 	i =0;
// 	while(i< 20){
// 		my_pthread_join(mythreads[i], NULL);
// 		i++;
// 	}
// 	my_pthread_mutex_destroy(mutex1);
// 	clock_gettime(CLOCK_MONOTONIC_RAW, &end);
// 	uint delta_us = (end.tv_sec - start.tv_sec)*1000000 +(end.tv_nsec - start.tv_nsec) / 1000;
// 
// 	printf("%d\n", delta_us);
//my_pthread_t mythread = NULL;
//int status = my_pthread_yield();
//printf("%d\n", status);
/*my_pthread_yield(mythread1);*/

	my_pthread_t mythread = 0;
	
	my_pthread_create(&mythread, NULL, (void*)returnNumber, NULL);
	
	void* ptr = NULL;
	
	my_pthread_join(mythread, &ptr);
	
	int* intPtr = (int*)ptr;

	printf("%d\n",*intPtr);

}