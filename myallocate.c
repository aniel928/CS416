#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "myallocate.h"
#include <unistd.h>

//QUESTIONS:
//1.should we take the first x bytes from the page to say dirty or not? Or do we have to give them the entire page?


static char memory[MEMORYSIZE];
//bzero(memory, MEMORYSIZE);

char* myallocate(int size, char* file, int line, int type){
	
	if(type){
		printf("Coming from library\n");
		//why handle this differently?
		
	}
	else{
		printf("Coming from macro\n");
		printf("page size: %d\n", PAGESIZE);
		//Asking for 0 bytes
		if(size == 0){
			printf("No bytes requested, file: %s, line: %d\n", file, line);
			return NULL;
		}
		else{
			printf("%d bytes requested\n", size);
		}
		//first spot free.
		if(memory[0] == 0){
			printf("it worked\n");
		}
		else{
			int i = 0;
			while(memory[i] != 0){
				i += PAGESIZE; //check page
			}
			
			printf("nope\n");
		}
	}
	return NULL;
}

void mydeallocate(char* ptr, char* file, int line, int type){
	printf("Free stuff\n");
}
int main(int argc, char** argv){
	printf("Can i have 5 bytes?\n");
	myallocate(5, __FILE__, __LINE__, 1);
//	malloc(5);	
	printf("thanks!\n");
}
