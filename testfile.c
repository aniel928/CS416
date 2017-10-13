#include "my_pthread.c"
/*
void testThreads(){
	printf("Printing\n");
	return;
}
*/
int main(int argc, char** argv){

my_pthread_t mythread1, mythread2, mythread3, mythread4, mythread5, mythread6;

my_pthread_create(&mythread1, NULL, (void*)&testThreads, NULL);
my_pthread_create(&mythread2, NULL, (void*)&testThreads, NULL);
my_pthread_create(&mythread3, NULL, (void*)&testThreads, NULL);
my_pthread_create(&mythread4, NULL, (void*)&testThreads, NULL);
my_pthread_create(&mythread5, NULL, (void*)&testThreads, NULL);
my_pthread_create(&mythread6, NULL, (void*)&testThreads, NULL);

}
