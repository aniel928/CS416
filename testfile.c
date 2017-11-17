
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


/* * * * * * * * * * 5 MB 200 times each thread, 64 threads * * * * * * * * * *

void giveMeMem(int* thread){
	printf("Thread #: %d\n", *thread);
	char* ptr[200];
	printf("mallocing 5MB\n");
	int i = 0;
	while(i < 200){
		ptr[i] = (char*)malloc(5*1024*1024);
		if(ptr[i]){
			printf("malloced %d pointer: %p\n",i, ptr);
			free(ptr[i]);
			printf("pointer %d freed\n", i);
			i++;
		}
		else{
			printf("-------POINTER IS NULL-------\n");
			return;
		}
	}

	pthread_exit(NULL);
	return;
}
*/

/* * * * * * * * * * 1 byte 1000 times each thread, free after allocating all 64 threads * * * * * * * * * 
void giveMeMem(int* thread){
	printf("Thread #: %d\n", *thread);
	char* ptr[200];
	printf("mallocing 5MB\n");
	int i = 0;
	while(i < 200){
		ptr[i] = (char*)malloc(1);
		if(ptr[i]){
			printf("malloced %d pointer: %p\n",i, ptr[i]);
//			free(ptr[i]);
//			printf("pointer %d freed\n", i);
			i++;
		}
		else{
			printf("-------POINTER IS NULL-------\n");
			return;
		}
	}
	
	i=0;
	while(i < 200){
		free(ptr[i]);
		printf("pointer %d freed\n", i);
		i++;
	}

	pthread_exit(NULL);
	return;
}
*/

/* * * * * * * * * * Shalloc * * * * * * * * * */
void giveMeMem(int* thread){
	printf("Thread #: %d\n", *thread);
	char* ptr[5];
	printf("mallocing 5MB\n");
	int i = 0;
	while(i < 5){
		ptr[i] = (char*)malloc(5);
		if(ptr[i]){
			printf("malloced %d pointer: %p\n",i, ptr[i]);
//			free(ptr[i]);
//			printf("pointer %d freed\n", i);
			i++;
		}
		else{
			printf("-------POINTER IS NULL-------\n");
			return;
		}
	}
	
	i=0;
	while(i < 5){
		free(ptr[i]);
		printf("pointer %d freed\n", i);
		i++;
	}

	pthread_exit(NULL);
	return;
}

void shallocIt(){
	char* myptr = (char*)shalloc(5);
	printf("myptr is %p\n", myptr);
	myptr[0] = 'A';
	myptr[1] = 'n';
	myptr[2] = 'n';
	myptr[3] = 'e';
	myptr[4] = '\0';
	
	printf("name will be: %s\n", myptr);
	pthread_exit(&myptr);
	
}

void useIt(void** ptr){
	printf("in Use it\n");
 	char* name = *ptr;
 	printf("My name is %s\n", name);
}


int main(int argc, char** argv){

/* * * * * Group tests * * * * */
// pthread_t mythread[5];
// int i=0;
// int j[5];
// while(i< 5){
// 	j[i] = i;
// 	pthread_create(&mythread[i], NULL, (void*)&giveMeMem, &j[i]);
// 	printf("thread: %d\n", i);
// 	i++;	
// 		
// }
// i=0;
// while(i< 5){
// 	pthread_join(mythread[i], NULL);
// 	i++;		
// }

char* ptr = (char*)malloc(5);
char* ptr1 = (char*)malloc(500);
char* ptr2 = (char*)malloc(5000);

/* * * * * Small one of tests * * * * */
// 	pthread_t mythread1, mythread2, mythread3;
// 	char* ptr1 = NULL;
// 	pthread_create(&mythread1, NULL, (void*)&shallocIt, NULL);
// 	pthread_join(mythread1, &ptr1);
// 
// 	printf("testing name: %s\n", ptr1);
// 	printf("testig pointer: %p\n", &ptr1);
// 
// 	pthread_create(&mythread2, NULL, (void*)&useIt, &ptr1);
// 	pthread_create(&mythread3, NULL, (void*)&useIt, &ptr1);	
// 	pthread_join(mythread2, NULL);
// 	pthread_join(mythread3, NULL);			

	printf("Test done\n");
//	showData();
}














// 
// #include "my_pthread_t.h"
// #include <time.h>
// #include <stdio.h>
// #include <stdlib.h>
// //#include <pthread.h>
// // #define NUM 30
// // pthread_mutex_t* mutex1 = NULL;
// 
//  
// // 
// // void returnNumber(){
// // 	int a = 4;
// // 	printf("YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY\n");
// // 	char* ptr = (char*)malloc(5000);
// // 	free(ptr);
// // 	pthread_exit((void*)&a);
// // }
// 
// 
// /* * * * * * * * * * 5 MB 200 times each thread, 64 threads * * * * * * * * * *
// 
// void giveMeMem(int* thread){
// 	printf("Thread #: %d\n", *thread);
// 	char* ptr[200];
// 	printf("mallocing 5MB\n");
// 	int i = 0;
// 	while(i < 200){
// 		ptr[i] = (char*)malloc(5*1024*1024);
// 		if(ptr[i]){
// 			printf("malloced %d pointer: %p\n",i, ptr);
// 			free(ptr[i]);
// 			printf("pointer %d freed\n", i);
// 			i++;
// 		}
// 		else{
// 			printf("-------POINTER IS NULL-------\n");
// 			return;
// 		}
// 	}
// 
// 	pthread_exit(NULL);
// 	return;
// }
// */
// 
// /* * * * * * * * * * 1 byte 1000 times each thread, free after allocating all 64 threads * * * * * * * * * 
// void giveMeMem(int* thread){
// 	printf("Thread #: %d\n", *thread);
// 	char* ptr[200];
// 	printf("mallocing 5MB\n");
// 	int i = 0;
// 	while(i < 200){
// 		ptr[i] = (char*)malloc(1);
// 		if(ptr[i]){
// 			printf("malloced %d pointer: %p\n",i, ptr[i]);
// //			free(ptr[i]);
// //			printf("pointer %d freed\n", i);
// 			i++;
// 		}
// 		else{
// 			printf("-------POINTER IS NULL-------\n");
// 			return;
// 		}
// 	}
// 	
// 	i=0;
// 	while(i < 200){
// 		free(ptr[i]);
// 		printf("pointer %d freed\n", i);
// 		i++;
// 	}
// 
// 	pthread_exit(NULL);
// 	return;
// }
// */
// 
// /* * * * * * * * * * Shalloc * * * * * * * * * */
// void giveMeMem(int* thread){
// 	printf("Thread #: %d\n", *thread);
// 	char* ptr[2];
// 	printf("mallocing 16000\n");
// 	int i = 0;
// 	while(i < 2){
// 		ptr[i] = (char*)malloc(16000);
// 		if(ptr[i]){
// //			printf("malloced %d pointer: %p\n",i, ptr[i]);
// //			free(ptr[i]);
// //			printf("pointer %d freed\n", i);
// 			i++;
// 		}
// 		else{
// 			printf("-------POINTER IS NULL-------\n");
// 			return;
// 		}
// 	}
// 	i = 0;
// 	while(i < 2){
// 		if(ptr[i]){
// //			printf("malloced %d pointer: %p\n",i, ptr[i]);
// 			free(ptr[i]);
// //			printf("pointer %d freed\n", i);
// 		
// 		}
// 			i++;
// 		
// 	}
// 	pthread_exit(NULL);
// 
// 	
// 	
// 	return;
// }
// 	
// 	//i=0;
// 	/*while(i < 3){
// 		free(ptr[i]);
// 		printf("pointer %d freed\n", i);
// 		i++;
// 	}*/
// //}
// 
// /*
// void* shallocIt(){
// 	char* myptr = NULL;
// 	myptr = (char*)shalloc(5);
// 	myptr[0] = 'A';
// 	myptr[1] = 'n';
// 	myptr[2] = 'n';
// 	myptr[3] = 'e';
// 	myptr[4] = '\0';
// 	
// 	pthread_exit(&myptr);
// }
// */
// void useIt(void** ptr){
// 		char * thing6 = (char*)myallocate(5000, __FILE__, __LINE__, 1);
// 		
// 		char * thing7 = (char*)myallocate(5000, __FILE__, __LINE__, 1);
// 		
// 		char * thing8 = (char*)myallocate(5000, __FILE__, __LINE__, 1);
// 		
// 		char * thing9 = (char*)myallocate(5000, __FILE__, __LINE__, 1);
// }
// 
// 
// int main(int argc, char** argv){
// 
// /* * * * * Group tests * * * * */
// 	pthread_t mythread[5];
//  	int i=0;
//  	int j[5];
//  	while(i< 5){
//  		j[i] = i;
// 		pthread_create(&mythread[i], NULL, (void*)&giveMeMem, &j[i]);
//  		printf("thread: %d\n", i);
//  		i++;		
//  	}
//  	i=0;
//  	while(i< 5){
//  		pthread_join(mythread[i], NULL);
//  		i++;		
//  	}
// 
// /* * * * * Small one of tests * * * * */
// 	pthread_t mythread1, mythread2, mythread3;
// 	char* ptr1 = NULL;
// 	//pthread_create(&mythread1, NULL, (void*)&shallocIt, NULL);
// 	//pthread_create(&mythread2, NULL, (void*)&useIt, &ptr1);
// 	//	pthread_create(&mythread3, NULL, (void*)&useIt, &ptr1);
// 	//	pthread_create(&mythread2, NULL, (void*)&useIt, &ptr1);
// 
// 		
// 	
// /*
// 		char * thing1 = (char*)malloc(5000);
// 		
// 		char * thing2 = (char*)malloc(5000);
// 		
// 		char * thing3 = (char*)malloc(5000);
// 		
// 		char * thing4 = (char*)malloc(5000);
// 		
// 		char * thing5 = (char*)malloc(5000);
// 		
// */
// 		
// 		//free (name);
// 
// 	
// 	
// //	pthread_join(mythread1, (void*)ptr1);
// //	pthread_join(mythread2, NULL);
// 	//pthread_join(mythread3, NULL);			
// 
// 	printf("Test done\n");
// 	dataEPT();
// 	printf("----\n");
// //	showData();
// }