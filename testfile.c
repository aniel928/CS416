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
	
	//printf("Thread #: %d\n", *thread);
	char* ptr[200];
	//printf("mallocing 5MB\n");
	int i = 0;
	/*while(i < 200){
		ptr[i] = (char*)malloc(5000);
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
	}*/
	char* sha = (char*)(shalloc(10));
	printf("shalloced\n");
	free(sha);
	/*	
	i=0;
	while(i < 200){
		free(ptr[i]);
		printf("pointer %d freed\n", i);
		i++;
	}*/
	

	pthread_exit(NULL);
	return;
}

void function(){
	//could not get this to work at all, I suck at the pthread create stuff
	//^^You just forgot to "join" the thread
	printf("in function\n");

	char* ptr1 = (char*)malloc(5*1024*1024);
	if(ptr1){
		printf("malloc successful? %p\n", ptr1);
	}
	else{
		printf("NULL\n");
	}

/*	char * str1 = (char*)shalloc(500);
	char * str2 = (char*)shalloc(300);
	char * str3 = (char*)shalloc(500);
	free(str2);
	char * str4 = (char*)shalloc(300);
	free(str1);
	char * str5 = (char*)shalloc(500);
	free(str3);
	free(str4);
	char * str6 = (char*)shalloc(300);
	char * str7 = (char*)shalloc(500);
	char * str8 = (char*)shalloc(300);
	char * str9 = (char*)shalloc(500);
	char * str10 = (char*)shalloc(300);
	free(str10);
	free(str9);
	free(str8);
	free(str7);
	free(str6);
	printf("functin done\n");
	pthread_exit(NULL);
*/	
	
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
/* * * * * Group tests * * * * *
	pthread_t mythread[30];
	int i=0;
	int j[30];
	while(i< 30){
		j[i] = i;
		pthread_create(&mythread[i], NULL, (void*)&giveMeMem, &j[i]);
		printf("thread: %d\n", i);
		i++;	
			
	}
	i=0;
	while(i< 30){
		pthread_join(mythread[i], NULL);
		i++;		
	}

* * * * * Small one of tests * * * * */
 	pthread_t mythread1, mythread2, mythread3;
/*	char* ptr1 = NULL;
	pthread_create(&mythread1, NULL, (void*)&shallocIt, NULL);
	pthread_join(mythread1, &ptr1);

	printf("testing name: %s\n", ptr1);
	printf("testig pointer: %p\n", &ptr1);

	pthread_create(&mythread2, NULL, (void*)&useIt, &ptr1);
	pthread_create(&mythread3, NULL, (void*)&useIt, &ptr1);	
	pthread_join(mythread2, NULL);
	pthread_join(mythread3, NULL);
*/
	printf("before mallocs\n");
	pthread_create(&mythread1, NULL, (void*)&function, NULL);
	pthread_join(mythread1, NULL);
//	char* ptr1 = (char*)malloc(5*1024*1024);
	printf("between mallocs\n");
//	char* ptr2 = malloc(5*1024*1024);
	pthread_create(&mythread2, NULL, (void*)&function, NULL);
	pthread_join(mythread2, NULL);
	printf("after mallocs\n");

			
/*	pthread_t mythread1;
	printf("before\n");
	pthread_create(&mythread1, NULL, (void*)&function, NULL);
	pthread_join(mythread1, NULL);//FTFY
	printf("out of join\n");
	char* new = (char*)shalloc(10);
	char* ptr1 = malloc(5000);
	printf("Hurray!\n");
	char* ptr2 = malloc(6000);
	free(ptr1);
	printf("getting here\n");
	char* ptr3 = malloc(5500);
	printf("apparently it's an issue with free\n");
	free(ptr2);
	printf("or not\n");
	char* ptr4 = malloc(8000);
	char* ptr5 = malloc(6000);
	free(ptr3);
	free(ptr4);
	free(ptr5);
	printf("about to free new\n");
	free(new);
	printf("after first set\n");
	char * ptrA = malloc(5000);
	free(ptrA);
	char * ptrB = malloc(10000);
	free(ptrB);
	printf("before shallocs\n");
	char * str1 = (char*)shalloc(500);
	char * str2 = (char*)shalloc(300);
	char * str3 = (char*)shalloc(500);
	free(str2);
	char * str4 = (char*)shalloc(300);
	free(str1);
	char * mstr1 = (char*)malloc(4000);
	char * str5 = (char*)shalloc(500);
	free(str3);
	free(str4);
	char * str6 = (char*)shalloc(300);
	char * str7 = (char*)shalloc(500);
	char * str8 = (char*)shalloc(300);
	char * str9 = (char*)shalloc(500);
	char * str10 = (char*)shalloc(300);
	free(str10);
	free(str9);
	free(str7);
	free(str8);
	free(str6);
	free(mstr1);
	printf("after last set\n");
*/	
// 	char* zero = 0x0;
// 	int a = *(zero);

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