#include "my_pthread_t.h"

void testThreads(){
	while(1){
	}
	return;
}

void testThreadsWithExit(){
	printf("Printing\n");
	my_pthread_exit(NULL);
	return;
}

int main(int argc, char** argv){

my_pthread_t mythread1, mythread2, mythread3, mythread4, mythread5, mythread6 = 0;

my_pthread_create(&mythread1, NULL, (void*)&testThreads, NULL);
printf("%d\n",mythread1);
my_pthread_create(&mythread2, NULL, (void*)&testThreads, NULL);
printf("%d\n",mythread2);
my_pthread_create(&mythread3, NULL, (void*)&testThreads, NULL);
printf("%d\n",mythread3);
my_pthread_create(&mythread4, NULL, (void*)&testThreads, NULL);
printf("%d\n",mythread4);
my_pthread_create(&mythread5, NULL, (void*)&testThreads, NULL);
printf("%d\n",mythread5);
my_pthread_create(&mythread6, NULL, (void*)&testThreads, NULL);
printf("%d\n",mythread6);
my_pthread_join(mythread1, NULL);
my_pthread_join(mythread2, NULL);
my_pthread_join(mythread3, NULL);
my_pthread_join(mythread4, NULL);
}
