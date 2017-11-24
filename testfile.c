#include "my_pthread_t.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>


int main(int argc, char** argv){

	//Mallocing and freeing 5mb 1000 times in a loop
	char* ptr = NULL;
	int i = 0;
	while(i< 1000){
		printf("%d\n", i);
		ptr = (char*)malloc(5*1024*1024);
		printf("%p\n", ptr);
		if(ptr){
			printf("free\n");
			free(ptr);
		}
		else{
			printf("oh no\n");
			exit(EXIT_FAILURE);
		}
		printf("i++\n");
		i++;
	}
	printf("Mic drop.");

}
