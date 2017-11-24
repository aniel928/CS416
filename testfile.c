
#include "my_pthread_t.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>



//Main Method Only
/*/Mallocing and freeing 5mb 10, 30, 50 times in a loop 
int main(int argc, char** argv){
	char* ptr = NULL;
	int i = 0;

//	while(i < 10){
	while(i < 30){
//	while(i < 50){
		printf("%d\n", i);
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

//Single Thread
/*/Same test as above, but creating one thread.
void function(){
	//paste each function from main in here.
}

int main(){
	pthread_t pthread = NULL;
	pthread_create(&pthread, NULL, (void*)&function, NULL);
	pthread_join(pthread, NULL);
	printf("finite\n");
}
// Outcome: idential*/

//Multiple Threads
/*/Allocating 5MB in 6 different threads
void function(){
	char* ptr = NULL;
	ptr = (char*)malloc(5*1024*1024);;
	if(ptr){
		printf("malloc successful: %p\n", ptr);
	}
	else{
		printf("malloc unsuccessful\n");
	}
//	printf("before yield\n");
	pthread_yield();
//	printf("before exit\n");
	pthread_exit(NULL);
//	printf("done with function\n");
}

int main(){
	pthread_t mythread1, mythread2, mythread3, mythread4, mythread5, mythread6 = NULL;
	pthread_create(&mythread1, NULL, (void*)&function, NULL);
	pthread_join(mythread1, NULL);
	pthread_create(&mythread2, NULL, (void*)&function, NULL);
	pthread_join(mythread2, NULL);
	pthread_create(&mythread3, NULL, (void*)&function, NULL);
	pthread_join(mythread3, NULL);
	pthread_create(&mythread4, NULL, (void*)&function, NULL);
	pthread_join(mythread4, NULL);
	pthread_create(&mythread5, NULL, (void*)&function, NULL);
	pthread_join(mythread5, NULL);
	pthread_create(&mythread6, NULL, (void*)&function, NULL);
	pthread_join(mythread6, NULL);	
	printf("goodbye sucker\n");
}
//Outcome: As long as you join each thread immediately after creation, runs perfectly. (Runs into some hiccups when threads are not joined immediately after create.*/

/*/Thread 1 malloc 5MB, thread 2 malloc 5MB, thread 1 frees, make sure thread 2 enters seg fault handler.
void function1(){
	char* ptr = NULL;
	ptr = (char*)malloc(5*1024*1024);
	my_pthread_yield();	
	free(ptr);
}
void function2(){
	char* ptr = NULL;
	ptr = (char*)malloc(5*1024*1024);
	my_pthread_yield();
	ptr[0] = 'A';
}
int main(int argc, char** argv){
	pthread_t mypthread1, mypthread2;
	pthread_create(&mypthread1, NULL, (void*)&function1, NULL);
	pthread_create(&mypthread2, NULL, (void*)&function2, NULL);
	pthread_join(mypthread1, NULL);
	pthread_join(mypthread2, NULL);
	printf("done\n");
}
//Outcome: works, properly segfaults and accesses correct page*/

//31 separate thread creates, each calling malloc of 5MB (more than half of the user space) and freeing 200 times.
void function(){
	printf("entering function\n");
	char* ptr = NULL;
	int i = 0;
	while(i < 200){
		ptr = (char*)malloc(5*1024*1024);
		if(ptr){
			printf("success! %p\n", ptr);
			free(ptr);
		}
		else{
			printf(":-(\n");
		}
		i++;
	}
	printf("exiting function\n");
}

int main(){
	pthread_t mythread[31];
	int i = 0;
	while(i < 31){
		pthread_create(&mythread[i], NULL, (void*)&function, NULL);
		i++;
	}
	i = 0;
	while(i < 31){
		pthread_join(mythread[i], NULL);
		i++;
	}
	printf("success?\n");
}

//31 threads, each calling malloc 200 times on 1 byte, do not free.


//31 threads, each calling malloc 200 times on 1 byte, do free.


/*/Shalloc
void function2(void* arg){
	char* name = (char*)arg;
	printf("%s\n", name);
	
}

int main(){
	pthread_t pthread = NULL;
	char* str = "nope";
	pthread_create(&pthread, NULL, (void*)&function2, (void*)str);
	pthread_join(pthread, NULL);
}//*/
