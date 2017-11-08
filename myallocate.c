#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "myallocate.h"
#include <unistd.h>

//QUESTIONS:
//1.should we take the first x bytes from the page to say dirty or not? Or do we have to give them the entire page? 
//If someone asks for 4090 bytes, and meta data is 16 bytes, how many pages? [2]
//Should there be data in front of all pages? what if someone requests 5500 bytes, do the pages get combined? only one metadata? [up to us, making only in front of first]
//New--
//Are we giving the extra room that was in the page but wasn't requested to other malloc calls? malloc calls from the same thread/type(?)? or are we just giving new malloc calls new pages
//If we're giving the leftover room to other calls, and say the first call frees, does that space that was freed become usable by new calls?
//if we're combining strings from different pages that aren't adjacent (only if this is necessary) then..
//do wereturn a concatenated string, pull the bytes from all the structs that are being used and concatenate them into a string to return
//Have to add--
//check to see if there is no room (in else)
//free the pointer that was given to free
//have to fix segfault in if else main, won't allow for mallocing more than there is
//when we implement swapping we shouldnt do it if the space left is smaller than the metadata size
static char memory[MEMORYSIZE];	//create huge memory buffer
memStruct * memHead = NULL;
memStruct * memNew = NULL;
memStruct * memFollow = NULL;
memStruct * memNext = NULL;
int pagesUsed = 0;

//This function is purely for testing, leaving in now but not necessary when we submit
showData(){
	memStruct * memTest = memHead;
	printf("----------PRINTING LINKED LIST DATA----------\n");
	int i = 0;
	int pc = 0;
	char* boolVal = "FALSE";
	if (memTest == NULL){
		printf("All empty?\n");
	}
	else{
		while (memTest!=NULL){
	
			if(memTest->inUse == TRUE){
				boolVal = "TRUE";
			}
			else{
				boolVal = "FALSE";
			}
			printf("Page number: %d, Page count: %d, inUse? %s, index: %d\n", i, memTest->pageCount, boolVal, pc*PAGESIZE);
			pc += memTest->pageCount;
			memTest = memTest->next;
			i++;
		}
	}
	printf("Size of memory: %d | Number of pages: %d | Page size: %d\n", MEMORYSIZE, NUMOFPAGES, PAGESIZE);
	printf("Bytes used: %d \t| Pages used: %d (including unused)\n", pc * PAGESIZE, usedPages());
	
}

//Checks to see how many pages have been used, need to switch to page table
int usedPages(){
	memStruct * memUP = memHead;
	int count = 0;
	while (memUP != NULL){
		count += memUP->pageCount;
		memUP = memUP->next;
	}
	return count;
}


char* myallocate(int size, char* file, int line, int type){
	char* pageArr[(NUMOFPAGES)]; 
	int usePgs= usedPages();
		
	//determining how many pages are needed to return
	int pageCount = ((size + sizeof(memStruct))/PAGESIZE) + 1;
	printf("Need %d pages for this Malloc\n", pageCount);
			
	//Check if head has been built
	if (memHead == NULL){	//memHead not built yet
		printf("Head is NULL\n");
		int a = 0;
		int index = 0;
		
		//creating indexes for each page and storing them in an array
		while (a <= NUMOFPAGES){
			pageArr[a] = &memory[index];
			index += PAGESIZE;
			a++;
		}
	
		//create head
		memHead = (memStruct*)memory;
		memHead->pageCount = pageCount;
		memHead->currUsed = size + sizeof(memStruct);
		memHead->inUse = TRUE;
		memHead->prev = NULL;
		memHead->next = NULL;
		
		//returning the front of the string
		int retAmt = PAGESIZE * pageCount - sizeof(memStruct); //not sure if I have to set the return string to this length, and if so it isn't working
		printf("returning %d bytes\n", retAmt);			
		void * retStr = (void*)(memory + sizeof(memStruct)-1);
		return retStr;
	}
	else{//head has been previously set
		printf("Head set, need %d pages\n", pageCount);
		memNew = memHead;
		memFollow = memNew;
		memNew = memNew->next;
		
		//find out how many pages have been used
		int totalPages = usedPages();
		
		//reset memNew
		memNew = memHead->next;
		int currPages = memHead->pageCount;
		int k = 0;
		while (memNew != NULL){
			if (memNew->inUse == FALSE && memNew->pageCount >= pageCount){ //may need to implement how many bytes are actually left
				break;
			}
			currPages += memNew->pageCount;
			memFollow = memNew;
			memNew = memNew->next;	
			k++;
		}
		if(memNew == NULL){ //found page is NULL
			//need to make sure that the available space can hold the requested size
			if ((usePgs + pageCount) > NUMOFPAGES){
				printf("Can't give this many bytes/pages for this malloc, returning\n");
				return NULL;
			}				
			memNew = (memStruct*)(memory +(totalPages * PAGESIZE - 1)); //This line is the cause of about an hour worth of segfaulting
			memFollow->next = memNew;
			memNew->inUse = TRUE;
			memNew->pageCount = pageCount;
			memNew->currUsed = sizeof(memStruct)+ size;
			memNew->prev = memFollow;
			memNew->next = NULL;
		}
		else{//found page that was previously used BUT it was free'd
			printf("found page that was previously used but it was freed %d\n", memNew->pageCount);
			memNew->inUse = TRUE;		//set back to in use
			memNew->currUsed = sizeof(memStruct)+size;
			//If the amount of pages that this previously used page is more than what is needed, create a new memStruct for the leftover pages and set its inUse to false
			if (memNew->pageCount > pageCount){
				//if the freed section has more pages than the pages needed, proceed accordingly
				//determine how many more pages there are
				int index = currPages * PAGESIZE - 1 + pageCount * PAGESIZE;
				memNext = (memStruct*)(memory +index);
				memNext->inUse = FALSE;
				memNext->pageCount = memNew->pageCount - pageCount;
				memNext->next = memNew->next;
				memNext->currUsed = sizeof(memStruct);
				memNext->prev = memNew;
				memNew->next = memNext;
				memNew->pageCount = pageCount;
			}
		}
		void* retStr = (void*)(memNew +sizeof(memStruct));
		return retStr;
	}
}
	
void mydeallocate(char* ptr, char* file, int line, int type){
	if (ptr == NULL){
		printf("Trying to free NULL\n");
		return;
	}
	printf("Free stuff\n");
	int a = 0;
	bool found = FALSE;		//will flip if found in loop
	int pageNum =  ((ptr +1  - memory)/PAGESIZE + 1)-1;
	int search = 0;
	memStruct * memFree = memHead;
	while (search <= pageNum && memFree != NULL){
		search += memFree->pageCount;
		if (search == pageNum){
			memFree->inUse = FALSE;
			found = TRUE;
			break;
		}		
		memFree = memFree->next;
	}
	if (found == FALSE){
		printf("could not find anything to free\n");
	}
	else{
		printf("Great success!\n");
	}
}
int main(int argc, char** argv){
	int i =0;
		printf("memory size: %d\n", MEMORYSIZE);
	  char	* tester1 = myallocate(1024*1024, __FILE__, __LINE__, 1);
		char	* tester2 = myallocate(1024*1024, __FILE__, __LINE__, 1);
		char	* tester3 = myallocate(1024*1024, __FILE__, __LINE__, 1);
		char	* tester4 = myallocate(1024*1024, __FILE__, __LINE__, 1);
		char	* tester5 = myallocate(1024*1024, __FILE__, __LINE__, 1);
		char	* tester6 = myallocate(1024*1024, __FILE__, __LINE__, 1);
		char	* tester7 = myallocate(1024*1024, __FILE__, __LINE__, 1);
		mydeallocate(tester4,__FILE__, __LINE__, 1);
		char	* tester8 = myallocate(300*300, __FILE__, __LINE__, 1);
		char * tester81 = myallocate (200*500, __FILE__, __LINE__, 1);
		char	* tester9 = myallocate(1024*1024, __FILE__, __LINE__, 1);
		char	* tester10 = myallocate(1024*1024, __FILE__, __LINE__, 1);
		char	* tester11 = myallocate(50*1024, __FILE__, __LINE__, 1);
		char	* tester12 = myallocate(1024*1024, __FILE__, __LINE__, 1);
		mydeallocate(tester3,__FILE__,__LINE__, 1);
		char	* tester13 = myallocate(1200*1024, __FILE__, __LINE__, 1);
		char	* tester14 = myallocate(500*1024, __FILE__, __LINE__, 1);
		mydeallocate(NULL,__FILE__,__LINE__, 1 );
							 
		printf("BACK\n");
		i++;
	showData();
	printf("thanks!\n");
	
}
