#include <stdlib.h>
#include <stdio.h>
#include "lossycount.h"
#include "prng.h"
#include <algorithm>
#include "plf_stack.h"

/********************************************************************
Implementation of Lazy Lossy Counting algorithm to Find Frequent Items
Based on the paper of Manku and Motwani, 2002
And Metwally, Agrwawal and El Abbadi, 2005
Implementation by G. Cormode 2002, 2003, 2005
This implements the space saving algorithm, which 
guarantees 1/epsilon space. 
This implementation uses a heap to track which is the current smallest count

Original Code: 2002-11
This version: 2002,2003,2005,2008

This work is licensed under the Creative Commons
Attribution-NonCommercial License. To view a copy of this license,
visit http://creativecommons.org/licenses/by-nc/1.0/ or send a letter
to Creative Commons, 559 Nathan Abbott Way, Stanford, California
94305, USA.
*********************************************************************/

#define LCL_NULLITEM 0x7FFFFFFF
	// 2^31 -1 as a special character

LCL_type * LCL_Init(float fPhi)
{
	int i;
	int k = 1 + (int) 1.0/fPhi;

	LCL_type *result = (LCL_type *) calloc(1,sizeof(LCL_type));
	// needs to be odd so that the heap always has either both children or 
	// no children present in the data structure
	result->size = (1 + k) | 1; // ensure that size is odd
	//result->size = (1 + k) | 3; // For minmax heap, ensure any node has either granchildren or no grandchild
	result->hashsize = LCL_HASHMULT*result->size;
	result->hashtable=(LCLCounter **) calloc(result->hashsize,sizeof(LCLCounter*));
	result->counters=(LCLCounter*) calloc(1+result->size,sizeof(LCLCounter));
	#if MAXHEAP
	result->maxheap=(LCLCounter **) calloc(1+result->size,sizeof(LCLCounter*));
	#endif
	// indexed from 1, so add 1

	result->hasha=151261303;
	result->hashb=6722461; // hard coded constants for the hash table,
	//should really generate these randomly
	result->error=(LCLweight_t) 0;

	for (i=1; i<=result->size;i++)
	{
		result->counters[i].next=NULL;
		result->counters[i].prev=NULL;
		result->counters[i].item=LCL_NULLITEM;
		#if MAXHEAP
		result->maxheap[i]=&(result->counters[i]);
		result->counters[i].maxheapptr=&(result->maxheap[i]);
		#endif
		// initialize items and counters to zero
	}
	result->root=&result->counters[1]; // put in a pointer to the top of the heap
	return(result);
}

void LCL_Destroy(LCL_type * lcl)
{
	free(lcl->hashtable);
	free(lcl->counters);
	free(lcl);
}

void LCL_RebuildHash(LCL_type * lcl)
{
	// rebuild the hash table and linked list pointers based on current
	// contents of the counters array
	int i;
	LCLCounter * pt;

	for (i=0; i<lcl->hashsize;i++)
		lcl->hashtable[i]=0;
	// first, reset the hash table
	for (i=1; i<=lcl->size;i++) {
		lcl->counters[i].next=NULL;
		lcl->counters[i].prev=NULL;
	}
	// empty out the linked list
	for (i=1; i<=lcl->size;i++) { // for each item in the data structure
		pt=&lcl->counters[i];
		pt->next=lcl->hashtable[lcl->counters[i].hash];
		if (pt->next)
			pt->next->prev=pt;
		lcl->hashtable[lcl->counters[i].hash]=pt;
	}
}

int MinHeapBubbleDown(LCL_type * lcl, int ptr)
{ // restore the heap condition in case it has been violated
	LCLCounter tmp;
	LCLCounter * cpt, *minchild;
	int mc;

	while(1)
	{
		if ((ptr<<1) + 1>lcl->size) break;
		// if the current node has no children

		cpt=&lcl->counters[ptr]; // create a current pointer
		mc=(ptr<<1)+
			((lcl->counters[ptr<<1].count<lcl->counters[(ptr<<1)+1].count)? 0 : 1);
		minchild=&lcl->counters[mc];
		// compute which child is the lesser of the two

		if (cpt->count < minchild->count) break;
		// if the parent is less than the smallest child, we can stop

		tmp=*cpt;
		*cpt=*minchild;
		*minchild=tmp;
		// else, swap the parent and child in the heap
		#if MAXHEAP
		lcl->maxheap[ cpt->maxheapptr - lcl->maxheap ] = cpt;
		lcl->maxheap[ minchild->maxheapptr - lcl->maxheap ] = minchild; 
		// Swap what pointers point to in max-heap also
		#endif



		if (cpt->hash==minchild->hash)
			// test if the hash value of a parent is the same as the 
			// hash value of its child
		{ 
			// swap the prev and next pointers back. 
			// if the two items are in the same linked list
			// this avoids some nasty buggy behaviour
			minchild->prev=cpt->prev;
			cpt->prev=tmp.prev;
			minchild->next=cpt->next;
			cpt->next=tmp.next;
		} else { // ensure that the pointers in the linked list are correct
			// check: hashtable has correct pointer (if prev ==0)
			if (!cpt->prev) { // if there is no previous pointer
				if (cpt->item!=LCL_NULLITEM)
					lcl->hashtable[cpt->hash]=cpt; // put in pointer from hashtable
			} else
				cpt->prev->next=cpt;
			if (cpt->next) 
				cpt->next->prev=cpt; // place in linked list

			if (!minchild->prev) // also fix up the child
				lcl->hashtable[minchild->hash]=minchild; 
			else
				minchild->prev->next=minchild; 
			if (minchild->next)
				minchild->next->prev=minchild;
		}
		ptr=mc;
		// continue on with the heapify from the child position
	} 
	//LCL_ShowHeap(lcl);
	return ptr;
}

/* max heap bubble up 
	Implemented using pointers to LCLCounter objects in lcl->counters.

*/
void MaxHeapBubbleUp(LCL_type * lcl, int ptr)
{ 
	LCLCounter * tmp;
	LCLCounter * cpt, *maxchild, *parent;
	LCLCounter ** tmpptr;
	int pr;
	while(1)
	{
		pr=(ptr>>1); // Parent index is floor(ptr/2) 
		if (pr < 1) break;
		// Stop if parent is out of bounds, array indexed from 1.

		parent = lcl->maxheap[pr];
		cpt = lcl->maxheap[ptr]; // create a current pointer

		if(parent->count > cpt->count) break;
		// If parent is larger than current pointer we are done

		lcl->maxheap[ptr] = parent;
		lcl->maxheap[pr] = cpt; 
		// Swap pointers in maxheap array
		
		parent->maxheapptr=&(lcl->maxheap[ptr]);
		cpt->maxheapptr=&(lcl->maxheap[pr]);
		// Swap the pointers in hashtable to maxheap translation

		ptr=pr;
		// continue on with the bubble-up from the parent position
	} 
}

int find_min_index(LCL_type * lcl, int ptr){
	int c1=(ptr<<2)+
			((lcl->counters[ptr<<2].count<lcl->counters[(ptr<<2)+1].count)? 0 : 1);
	int c2=(ptr<<2)+
		((lcl->counters[ptr<<2+2].count<lcl->counters[(ptr<<2)+3].count)? 2 : 3);
	return ((lcl->counters[c1].count<lcl->counters[c2].count)? c1 : c2);
}

int find_max_index(LCL_type * lcl, int ptr){
	int c1=(ptr<<2)+
			((lcl->counters[ptr<<2].count>lcl->counters[(ptr<<2)+1].count)? 0 : 1);
	int c2=(ptr<<2)+
		((lcl->counters[ptr<<2+2].count>lcl->counters[(ptr<<2)+3].count)? 2 : 3);
	return ((lcl->counters[c1].count>lcl->counters[c2].count)? c1 : c2);
}

void PushDownMin(LCL_type * lcl, int ptr){
	LCLCounter tmp;
	LCLCounter * cpt, *minchild;
	int mc;
	bool gc=false;
	while(1){
		if ((ptr<<1) + 1>lcl->size) break; // if no children, break
		cpt=&lcl->counters[ptr];
		if (!(((ptr<<2) + 3)>lcl->size)) { // if grandchild exists
			mc=find_min_index(lcl,ptr);
			minchild=&lcl->counters[mc];
			gc=true;
		}
		else{
			mc=(ptr<<1)+
				((lcl->counters[ptr<<1].count<lcl->counters[(ptr<<1)+1].count)? 0 : 1);
			minchild=&lcl->counters[mc];
		}
		if (lcl->counters[mc].count < lcl->counters[ptr].count){
			// swap
			tmp=*cpt;
			*cpt=*minchild;
			*minchild=tmp;
		}
		else{
			break;
		}
		if (gc){
			if(lcl->counters[mc].count > lcl->counters[mc>>1].count){
				// swap gc
				cpt=&lcl->counters[mc];
				minchild=&lcl->counters[mc>>1];
 				tmp=*cpt;
				*cpt=*minchild;
				*minchild=tmp;
			}
		}
		ptr=mc;
	}
}
void PushDownMax(LCL_type * lcl, int ptr){
	LCLCounter tmp;
	LCLCounter * cpt, *maxchild;
	int mc;
	bool gc=false;
	while(1){
		if ((ptr<<1) + 1>lcl->size) break; // if no children, break

		cpt=&lcl->counters[ptr];

		if (!(((ptr<<2) + 3)>lcl->size)) { // if grandchild exists
			mc=find_max_index(lcl,ptr);
			maxchild=&lcl->counters[mc];
			gc=true;
		}
		else{
			mc=(ptr<<1)+
				((lcl->counters[ptr<<1].count>lcl->counters[(ptr<<1)+1].count)? 0 : 1);
			maxchild=&lcl->counters[mc];
		}
		if (lcl->counters[mc].count > lcl->counters[ptr].count){
			// swap
			tmp=*cpt;
			*cpt=*maxchild;
			*maxchild=tmp;
		}
		else{
			break;
		}
		if (gc){
			if(lcl->counters[mc].count < lcl->counters[mc>>1].count){
				// swap gc
				cpt=&lcl->counters[mc];
				maxchild=&lcl->counters[mc>>1];
 				tmp=*cpt;
				*cpt=*maxchild;
				*maxchild=tmp;
			}
		}
		ptr=mc;
	}
}

void MinMaxHeapPushDown(LCL_type * lcl, int ptr){
	// node level is ptr mod 2, even level is min, odd is max
	((31-__builtin_clz(ptr)) % 2) ? PushDownMax(lcl,ptr) : PushDownMin(lcl,ptr);
}


LCLCounter * LCL_FindItem(LCL_type * lcl, LCLitem_t item)
{ // find a particular item in the date structure and return a pointer to it
	LCLCounter * hashptr;
	int hashval;

	hashval=(int) hash31(lcl->hasha, lcl->hashb,item) % lcl->hashsize;
	if (hashval == 0){
		hashval=1;
	}
	hashptr=lcl->hashtable[hashval];
	// compute the hash value of the item, and begin to look for it in 
	// the hash table

	while (hashptr) {
		if (hashptr->item==item)
			break;
		else hashptr=hashptr->next;
	}
	return hashptr;
	// returns NULL if we do not find the item
}

void LCL_Update(LCL_type * lcl, LCLitem_t item, LCLweight_t value)
{
	int hashval,bubbleDownPos;
	LCLCounter * hashptr;
	// find whether new item is already stored, if so store it and add one
	// update heap property if necessary

	lcl->counters->item=0; // mark data structure as 'dirty'
	hashval=(int) hash31(lcl->hasha, lcl->hashb,item) % lcl->hashsize;
	if (hashval == 0){
		hashval=1;
	}
	hashptr=lcl->hashtable[hashval];
	
	// compute the hash value of the item, and begin to look for it in 
	// the hash table

	while (hashptr) {
		if (hashptr->item==item) {
			//maxheapptr = hashptr->maxheapptr;
			hashptr->count+=value; // increment the count of the item
			bubbleDownPos=MinHeapBubbleDown(lcl,hashptr - lcl->counters); // and fix up the heap
			#if MAXHEAP
			MaxHeapBubbleUp(lcl,(lcl->counters[bubbleDownPos].maxheapptr) - lcl->maxheap); // fix up maxheap
			#endif
			return;
		}
		else hashptr=hashptr->next;
	}
	// if control reaches here, then we have failed to find the item
	// so, overwrite smallest heap item and reheapify if necessary
	// fix up linked list from hashtable
	if (!lcl->root->prev) // if it is first in its list
		lcl->hashtable[lcl->root->hash]=lcl->root->next;
	else
		lcl->root->prev->next=lcl->root->next;
	if (lcl->root->next) // if it is not last in the list
		lcl->root->next->prev=lcl->root->prev;
	// update the hash table appropriately to remove the old item

	// slot new item into hashtable
	hashptr=lcl->hashtable[hashval];
	lcl->root->next=hashptr;
	if (hashptr)
		hashptr->prev=lcl->root;
	lcl->hashtable[hashval]=lcl->root;
	// we overwrite the smallest item stored, so we look in the root
	lcl->root->prev=NULL;
	lcl->root->item=item;
	lcl->root->hash=hashval;
	lcl->error=lcl->root->count;
	// update the implicit lower bound on the items frequency
	//  value+=lcl->root->delta;
	// update the upper bound on the items frequency
	lcl->root->count=value+lcl->error;
	bubbleDownPos=MinHeapBubbleDown(lcl,1); // restore heap property if needed
	#if MAXHEAP
	MaxHeapBubbleUp(lcl,(lcl->counters[bubbleDownPos].maxheapptr) - lcl->maxheap); //fix max-heap, bubble up from pos of min-heap bubble down stopped
	#endif
	// return value;
}

int LCL_Size(LCL_type * lcl)
{ // return the size of the data structure in bytes
	return sizeof(LCL_type) + (lcl->hashsize * sizeof(int)) + 
		(lcl->size * sizeof(int)) + // size of maxheap 
		(lcl->size*sizeof(LCLCounter));
}

LCLweight_t LCL_PointEst(LCL_type * lcl, LCLitem_t item)
{ // estimate the count of a particular item
	LCLCounter * i;
	i=LCL_FindItem(lcl,item);
	if (i)
		return(i->count);
	else
		return 0;
}

LCLweight_t LCL_PointErr(LCL_type * lcl, LCLitem_t item)
{ // estimate the worst case error in the estimate of a particular item
 // Modified, this implementation does not track per-element error
	return lcl->error;
}

int LCL_cmp( const void * a, const void * b) {
	LCLCounter * x = (LCLCounter*) a;
	LCLCounter * y = (LCLCounter*) b;
	if (x->count<y->count) return -1;
	else if (x->count>y->count) return 1;
	else return 0;
}

void LCL_Output(LCL_type * lcl) { // prepare for output
	if (lcl->counters->item==0) {
		qsort(&lcl->counters[1],lcl->size,sizeof(LCLCounter),LCL_cmp);
		LCL_RebuildHash(lcl);
		lcl->counters->item=1;
	}
}

// Output for Delegation Space-Saving
void LCL_Output(LCL_type * lcl, int thresh,std::vector<std::pair<uint32_t,uint32_t>>* v)
{
	#if MAXHEAP
	int curr_node;
	plf::stack<int> stack;
	stack.push(1); // 1 is root
	while(!stack.empty()){
		curr_node=stack.top();
		stack.pop();
		if (lcl->maxheap[curr_node]->count >= thresh){
			// add children to stack
			if (! (((curr_node<<1) + 1) > lcl->size)){ // if not a leaf, add children
				stack.push(curr_node<<1);
				stack.push((curr_node<<1)+1);
			}
			v->push_back(std::make_pair((uint32_t)lcl->maxheap[curr_node]->item,lcl->maxheap[curr_node]->count));
		}
	}
	#else
	const int size=lcl->size;
	for (int i=1;i<=size;++i){
		if (lcl->counters[i].count >= thresh){
			v->push_back(std::make_pair((uint32_t)lcl->counters[i].item,lcl->counters[i].count));
		}
	}
	#endif
}

void LCL_ShowHeap(LCL_type * lcl)
{ // debugging routine to show the heap
	int i, j;
	int level=0;
	printf("Min-heap tree:\n");
	printf("level:%d\n",level);
	j=1;
	for (i=1; i<=lcl->size; i++)
	{
		printf("%d ",(int) lcl->counters[i].count);
		if (i==j) 
		{ 
			level++;
			printf("\n");
			printf("level:%d\n",level);
			j=2*j+1;
		}
	}
	printf("\nMin-heap array:\n");
	printf("Elem:  ");
	for (i=1; i<=lcl->size; i++){
		printf("%d ",lcl->counters[i].item);
	}
	printf("\nCount: ");
	for (i=1; i<=lcl->size; i++){
		printf("%d ",lcl->counters[i].count);
	}
	printf("\n");
}
