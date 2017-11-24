#include "my_pthread_t.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

//Mallocing and freeing 5mb 10, 30, 50 times in a loop 
int main(int argc, char** argv){
	char* ptr = NULL;
	int i = 0;

//	while(i < 10){
	while(i < 30){
//	while(i < 50){
		ptr = (char*)malloc(5*1024*1024);
		if(ptr){
			printf("malloc successful: %p\n", ptr);
			free(ptr);
		}
		else{
			printf("malloc unsuccessful\n");
		}
		i++;
	} 
	printf("done\n");
}
//Outcome: once we start approaching 50 times in a loop, we start getting a stack overflow issue in the OS region.*/

/*/Mallocing 2MB 10 times and accessing the data in the first call after the last.
int main(int argc, char** argv){
	char* ptr[10] = {NULL};
	int i = 0;
	while (i < 10){
		ptr[i] = (char*)malloc(2*1024*1024);
		if(ptr[i]){
			printf("malloc successful: %p\n", ptr[i]);
		}
		else{
			printf("malloc unsuccesful\n");
		}
		i++;
	}
	ptr[0][0] = 'h';
	printf("done\n");
}
//Outcome: first 3 malloc calls are successful, last 7 are not (which is correct, a thread cannot take up more than the entire userspace according to spec). No issue using first malloced space.*/

/*/One malloc call for 6mb followed by a free and then 1000 malloc calls for 6000 bytes
int main(int argc, char** argv){
	char* ptr = NULL; //for 6MB
	ptr = (char*)malloc(6*1024*1024);
	if(ptr){
		printf("malloc successful: %p\n", ptr);
		free(ptr);
	}
	else{
		printf("malloc unsuccessful\n");
	}
	//ptrs for individual
	char* ptrs[1000] = {NULL};
	int i = 0;
	while(i < 1000){
		ptrs[i]=(char*)malloc(6000);
		if(ptrs[i]){
			printf("malloc successful: %p\n", ptrs[i]);
		}
		else{
			printf("malloc unsuccessful on %d\n", i);
		}
		i++;	
	}
}
//Outcome: successfully works for first 894 calls, but then memory fills up and the rest are appropriately returned null pointers.*/


/*/One malloc call for 6000 bytes followed by a free and then 6 malloc calls for 1000 bytes
int main(int argc, char** argv){
	char* ptr = NULL; //for 6MB
	ptr = (char*)malloc(6000);
	if(ptr){
		printf("malloc successful: %p\n", ptr);
		free(ptr);
	}
	else{
		printf("malloc unsuccessful\n");
	}
	//ptrs for individual
	char* ptrs[6] = {NULL};
	int i = 0;
	while(i < 6){
		ptrs[i]=(char*)malloc(1000);
		if(ptrs[i]){
			printf("malloc successful: %p\n", ptrs[i]);
		}
		else{
			printf("malloc unsuccessful on %d\n", i);
		}
		i++;	
	}
}

//Outcome: successfully frees and then fits the next 4 calls for 1000 bytes into the first page, then moves on to next page for next two calls.*/
 
/*/Allocate (pagesize - sizeof(memStruct) - (5 + sizeof(memStruct)), then allocate 5 bytes twice.
int main(int argc, char** argv){
	char* ptr = NULL;
	ptr = (char*)malloc(4096 - 24 - (5 + 24));
	char* ptr1 = NULL;
	ptr1 = (char*)malloc(5);
	char* ptr2 = NULL;
	ptr2 = (char*)malloc(5);
}
//Outcome: First 5 byte call properly consumed the rest of first page, second one properly went into new page.*/


/*/Allocate (pagesize - sizeof(memStruct) - (5 + sizeof(memStruct)), then allocate 1 byte twice.
int main(int argc, char** argv){
	char* ptr = NULL;
	ptr = (char*)malloc(4096 - 24 - (5 + 24));
	char* ptr1 = NULL;
	ptr1 = (char*)malloc(1);
	char* ptr2 = NULL;
	ptr2 = (char*)malloc(1);
}
//Outcome: First 1 byte call properly consumed the rest of first page, second one properly went into new page.*/
