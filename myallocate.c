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

static char memory[MEMORYSIZE];	//create huge memory buffer
memStruct * memHead = NULL;
memStruct * memNew = NULL;
memStruct * memFollow = NULL;
memStruct * memNext = NULL;
int pagesUsed = 0;

showData(){
	memStruct * memTest = memHead;
	printf("----------PRINTING LINKED LIST DATA----------\n");
	int i = 0;
	char* boolVal = "FALSE";
	if (memTest == NULL){
		printf("what?\n");
	}
	else{
		while (memTest!=NULL){
			if(memTest->inUse == TRUE){
				boolVal = "TRUE";
			}
			else{
				boolVal = "FALSE";
			}
			printf("Page number: %d, Page count: %d, inUse? %s, Addr: %d\n", i, memTest->pageCount, boolVal, memTest);
			memTest = memTest->next;
			i++;
		}
	}
}

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
		printf("in myallocate\n");	
		//I think this is going to cause an issue because I wanted to make it global but it wasn't letting memHead
		//I tested something and it actually seems to be working fine but I'm leaving this here for now just in case [Don't even know if this array is necessary]
		char* pageArr[(NUMOFPAGES)]; 
		
		int usePgs= usedPages();
		printf("Total amount of pages: %d, pages used so far: %d\n", NUMOFPAGES, usePgs);
		

		
		//determining how many pages are needed to return
		int pageCount = ((size + sizeof(memStruct))/PAGESIZE) + 1;
		printf("Need %d pages for this Malloc\n", pageCount);
				
		//Check if head has been built
		if (memHead == NULL){	//memHead not built yet
			printf("Head is NULL\n");
			int a = 0;
			int index = 0;
			
			//creating indexes for each page and storing them in an array
			while (a <= NUMOFPAGES){//index <=MEMORYSIZE - (PAGESIZE/2)){
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
			char * retStr = memory + sizeof(memStruct)-1;
			return retStr;
		}
		else{//head has been previously set
			printf("Head set, need %d pages\n", pageCount);
			memNew = memHead;
			memFollow = memNew;
			memNew = memNew->next;
			
			//find out how many pages have been used
			int totalPages = memHead->pageCount;	
			while (memNew!=NULL){
					totalPages += memNew->pageCount;
					memNew = memNew->next;
			}
			//find next available page
			memNew = memFollow->next;
		
		while (memNew != NULL){
			if (memNew->inUse == FALSE && memNew->pageCount >= pageCount){
				break;
			}
			memFollow = memNew;
			memNew = memNew->next;		
		}
		if(memNew == NULL){ //found page is NULL
			//need to make sure that the available space can hold the requested size
				if ((usePgs + pageCount) > NUMOFPAGES){
					printf("Can't give this many bytes/pages for this malloc, returning\n");
					return ;
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
			memNew->inUse = TRUE;		//set back to in use
			memNew->currUsed = sizeof(memStruct)+size;
			//If the amount of pages that this previously used page is more than what is needed, create a new memStruct for the leftover pages and set its inUse to false
			if (memNew->pageCount > pageCount){
				//cant get memNext set to the right value
				int index = (int)memNew;
				printf("index: %d\n", index);
				index = index + pageCount * PAGESIZE;
				index = memory - index;
				printf("index: %d\n");
				printf("this should be the index for the next.. %d\n", index);
				//(memNew->pageCount - pageCount) * PAGESIZE;
				printf("Creating a new node for extra pages, value of index: %d\n", index);
				memNext = (memStruct*)(memory + index);
				printf("current index: %d, the next is %d\n", memNew, memNext);
				memNext->inUse = FALSE;
				printf("1\n");
				memNext->pageCount = memNew->pageCount - pageCount;
				printf("2\n");
				memNext->next = memNew->next;
				printf("3\n");
				memNext->prev = memNew;
				printf("4\n");
				memNew->next = memNext;
				printf("5\n");
				memNew->currUsed = sizeof(memStruct);
				printf("finished\n");
			}
		}
		//printf("Need this number: %d, Have these: %d | %d | %d\n", (memory +(totalPages * PAGESIZE - 1)), &memNew, memNew, *memNew);
		char* retStr = (char*)(memNew +sizeof(memStruct));
		
		//+ sizeof(memStruct));//(memory + sizeof(memStruct)+(totalPages * PAGESIZE - 1));
		//printf("This is the address being returned: %d size: %d\n For some reason its adding 1024 and not 32 like it should be\n", retStr, sizeof(memStruct));
		return retStr;
	}
}
	
void mydeallocate(char* ptr, char* file, int line, int type){
	printf("Free stuff\n");
	int a = 0;
	bool found = FALSE;		//will flip if found in loop
	int pageNum =  ((ptr +1  - memory)/PAGESIZE + 1)-1;
	int search = 0;
	memStruct * memFree = memHead;
	printf("&ptr = %d, &memory = %d,result is this: %d\n", ptr+1, memory, pageNum);
	printf("mfpc: %d",memFree->pageCount);
	while (search <= pageNum && memFree != NULL){
		printf("search: %d pageNum: %d\n", search, pageNum);
		search += memFree->pageCount;
		printf("value of search: %d, (just added %d)\n", search, memFree->pageCount);
		if (search == pageNum){
			printf("address being freed: %d\n", memFree);
			memFree->inUse = FALSE;
			found = TRUE;
			printf("By George, I think I've got it.\n");
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
	/*rintf("Can i have 4090 bytes?\n");
	char* test1 = myallocate(4090, __FILE__, __LINE__, 1);
	
	char* test2 = myallocate(3,  __FILE__, __LINE__, 1);
	char* test3 = myallocate(5000, __FILE__, __LINE__, 1);
	mydeallocate(test3, __FILE__, __LINE__, 1);
	char* test4 = myallocate(100, __FILE__, __LINE__, 1);
	
	char* test5 = myallocate(300,  __FILE__, __LINE__, 1);
	char* test6 = myallocate(5000, __FILE__, __LINE__, 1);
	*/
//	char** tester;
	int i =0;
	//while (i<100){
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
	//	char * tester9 = myallocate (200*500, __FILE__, __LINE__, 1);
	/*	char	* tester9 = myallocate(1024*1024, __FILE__, __LINE__, 1);
		char	* tester10 = myallocate(1024*1024, __FILE__, __LINE__, 1);
		char	* tester11 = myallocate(1024*1024, __FILE__, __LINE__, 1);
		char	* tester12 = myallocate(1024*1024, __FILE__, __LINE__, 1);
							 
		printf("BACK\n");
		i++;
//	}
//	malloc(5);*/

showData();

printf("thanks!\n");
	
	
	
	
	
	
}





/**Dumb Code**/


			/*Was used for finding the next free node but don't think I need it
				memStruct * findNext(){
				memStruct * memSearch = memHead;
				if (memHead->next == NULL){
				printf("next is NULL, create another\n");
			}
			else{
				printf("next is not NULL, search for first NULL\n");
				while(memSearch != NULL){
				printf("in while for findNext\n");
				if (memSearch->inUse == FALSE){
					break;
				}
				printf("before this line\n");
				memSearch = memSearch->next;	
				printf("after this line\n");
			}
		}
	printf("out of while\n");
	return memSearch;
}*/

	
			/* This was for adding a new struct after head and setting the inUse to false, but this isn't necessary
			memNew = (memStruct*)memory+(pageCount * PAGESIZE - 1);
			//memNew->sizeLeft = 0;
			memNew->pageCount = 0;
			memNew->currUsed = sizeof(memStruct);
			memNew->inUse == FALSE;
			memNew->prev = memHead;
			memNew->next = NULL;
			*/

		//this is for determining the pages needed if EVERY page were to have metadata,
		//took it out because it only puts metadata in front of the first page
		/*while (pageCheck + sizeof(memStruct) >= PAGESIZE){
			pageCheck = pageCheck + sizeof(memStruct) - PAGESIZE;	
			pageCount++;			
			}*/
	
			/*false
			printf("head HAS been set\n");
			memStruct * new;
			new->sizeLeft = PAGESIZE * pageCount - sizeof(memStruct);
			new->inUse = TRUE;
			memStruct * temp = memHead;
			int i = 1;
			while (temp->next!= NULL){
				temp = temp->next;
				printf("moved to next\n");
				i++;
			} 
			printf("lasdkfjalsd\n");
			new->next = NULL;
			new->prev = temp;
			temp->next = new;
			printf("built and set new, number %d\n", i);*/
			
			
			/* This was only useful if we added metadata in front of all pages (I think)
			memStruct * tempNext = memHead;
			int i = 1;
			while (i < pageCount){
				printf("Need more pages, so searching for next free %d\n", i);
				tempNext->next = findNext();				
				i++;
			}
			*/
			
/**Annes Code**/
/*if(type){
		printf("Coming from library\n");
		//why handle this differently?
	}
	else{
		printf("Coming from macro\n");
		printf("page size: %d\n", PAGESIZE);
		//Asking for 0 bytes
		if(size == 0){	//mallocing 0 bytes
			printf("No bytes requested, file: %s, line: %d\n", file, line);
			return NULL;
		}
		else{	//mallocing > 0 bytes
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
	return NULL;*/