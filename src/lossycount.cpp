#include <stdlib.h>
#include <stdio.h>
#include "lossycount.h"
#include "prng.h"
#include <algorithm>
#include <functional>
#include <cstdint>
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
Modified by Victor Jarlow 2022, full history available at:
https://github.com/jarlow/Delegation-Space-Saving

This work is licensed under the Creative Commons
Attribution-NonCommercial License. To view a copy of this license,
visit http://creativecommons.org/licenses/by-nc/1.0/ or send a letter
to Creative Commons, 559 Nathan Abbott Way, Stanford, California
94305, USA.
*********************************************************************/

constexpr LCLweight_t LCL_NULLITEM = 0x7FFFFFFF;
// 2^31 -1 as a special character

// *********** Helper functions ***********
static inline uint32_t integerLog2(const uint32_t x) {
  uint32_t y;
  asm ( "\tbsr %1, %0\n"
      : "=r"(y)
      : "r" (x)
  );
  return y;
}

static inline bool isMinLevel(const int ptr, const LCL_type *lcl) {
	return (lcl->modtable[integerLog2(ptr)]);
	// even levels are min, odd ones are max
}

static inline bool isMaxLevel(const int ptr, const LCL_type *lcl) {
	return isMinLevel(ptr,lcl)^1;
}

template <typename Comparator>
static inline int minMaxChild(const LCL_type* lcl, const int ptr, const Comparator comp) {
	int ch = ptr<<1;
	return(ch)+
			(comp(lcl->counters[ch].count,lcl->counters[ch+1].count) ? 0 : 1);
}

template <typename Comparator>
static inline int minMaxGrandchild(const LCL_type* lcl, const int ptr, const Comparator comp) {
	int gc = ptr<<2;
	int c1 = gc +
			(comp(lcl->counters[gc].count,lcl->counters[gc+1].count) ? 0 : 1);
	int c2 = gc +
		(comp(lcl->counters[gc+2].count,lcl->counters[gc+3].count) ? 2 : 3);
	return (lcl->counters[c1].count < lcl->counters[c2].count) ? c1 : c2;
}

static inline bool hasGrandchildren(const int index, const int size) {
	return (index << 2) <= size + 1;
}

static inline bool hasChildren(const int index, const int size) {
	return (index << 1) <= size + 1;
}

static inline bool hasGrandparent(const int index, const int size) {
	return (index >> 2) > 0;
}

static inline void swapAndMaintainHashtable(LCLCounter *one, LCLCounter *other, const LCL_type *lcl){
	LCLCounter tmp;
	tmp = *one;
	*one=*other;
	*other=tmp;

	if (one->hash==other->hash){
		// test if the hash value of a parent is the same as the 
		// hash value of its child

		// swap the prev and next pointers back. 
		// if the two items are in the same linked list
		// this avoids some nasty buggy behaviour
		other->prev=one->prev;
		one->prev=tmp.prev;
		other->next=one->next;
		one->next=tmp.next;
	} 
	else { 
		// ensure that the pointers in the linked list are correct
		// check: hashtable has correct pointer (if prev ==0)
		if (!one->prev) { // if there is no previous pointer
			if (one->item!=LCL_NULLITEM)
				lcl->hashtable[one->hash]=one; // put in pointer from hashtable
		} 
		else {one->prev->next=one;}
		if (one->next) {one->next->prev=one;} // place in linked list
		if (!other->prev){lcl->hashtable[other->hash]=other;} // also fix up the child
		else{other->prev->next=other;}
		if (other->next) {other->next->prev=other;}
	}
}

// *********** Implementation ***********

LCL_type * LCL_Init(const float fPhi)
{
	int i;
	int k = 1 + (int) 1.0/fPhi;

	LCL_type *result = (LCL_type *) calloc(1,sizeof(LCL_type));
	#if MINMAXHEAP
	result->size = (1 + k) | 3; // ensure a node has either all granchildren or no grandchild
	#else
	result->size = (1 + k) | 1; // ensure that a node either all children or no child
	#endif

	result->hashsize = LCL_HASHMULT*result->size;
	result->hashtable=(LCLCounter **) calloc(result->hashsize,sizeof(LCLCounter*));
	result->counters=(LCLCounter*) calloc(1+result->size,sizeof(LCLCounter)); // indexed from 1, so add 1


	result->hasha=151261303;
	result->hashb=6722461; // hard coded constants for the hash table,
	//should really generate these randomly
	result->error=(LCLweight_t) 0;

	for (i=1; i<=result->size;i++)
	{
		result->counters[i].next=NULL;
		result->counters[i].prev=NULL;
		result->counters[i].item=LCL_NULLITEM;
		// initialize items and counters to zero
	}
	result->modtable = (bool*) calloc(integerLog2(result->size),sizeof(bool));
	for (i=0; i<integerLog2(result->size); i+=2)
		result->modtable[i] = true;
	// initialize the modulo lookup table (1 on even levels, 0 on odd levels))	
	result->root=&result->counters[1]; // put in a pointer to the top of the heap
	return result;
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

void MinHeapBubbleDown(const LCL_type * lcl, int ptr)
{ // restore the heap condition in case it has been violated
	LCLCounter *cpt = nullptr, *minchild = nullptr;
	int mc = -1;

	while(1)
	{
		if ((ptr<<1) + 1 > lcl->size) break;
		// if the current node has no children

		mc = minMaxChild(lcl, ptr, std::less<volatile LCLweight_t>());
		cpt=&lcl->counters[ptr]; // create a current pointer
		minchild=&lcl->counters[mc]; // create a pointer to the child with the smallest count

		if (cpt->count < minchild->count) break;
		// if the parent is less than the smallest child, we can stop

		swapAndMaintainHashtable(cpt,minchild,lcl);
		ptr=mc;
		// continue on bubbling down from the child position
	}
}

template <typename Comparator>
LCLCounter* MinMaxHeapPushDown(const LCL_type * lcl, int ptr, const Comparator comp){
	LCLCounter tmp;
	LCLCounter *cpt,*mchild,*mgchild;
	cpt = mchild = mgchild = nullptr;
	int mc = -1, mgc = -1;
	cpt=&(lcl->counters[ptr]);

	if ((ptr<<1) + 1 > lcl->size) return cpt; // if no children, return

	if (((ptr<<2) + 3) <= lcl->size) { // if grandchild exists
		mgc = minMaxGrandchild(lcl,ptr,comp);
		mgchild=&(lcl->counters[mgc]);
	}

	mc = minMaxChild(lcl,ptr,comp);
	mchild=&(lcl->counters[mc]);

	if (mgchild && comp(mgchild->count,mchild->count)){ // (min/max)-grandchild exists, and is smaller/larger than smallest child
		if (comp(mgchild->count,cpt->count)){ // if (min/max)-grandchild is smaller/larger than current pointer
			swapAndMaintainHashtable(mgchild,cpt,lcl); // swap pointers
			if (!comp(mgchild->count,lcl->counters[mgc>>1].count)){ // if (min/max)-grandchild is larger/smaller than its parent
				swapAndMaintainHashtable(mgchild,&(lcl->counters[mgc>>1]),lcl); // swap with parent
			}
			return MinMaxHeapPushDown(lcl,mgc,comp); // push down grandchild
		}
	}
	else if (comp(mchild->count,cpt->count)){ // if (min/max)-child is smaller/larger than current pointer
			swapAndMaintainHashtable(mchild,cpt,lcl); // swap pointers
	}
	return cpt;
}

LCLCounter* MinMaxHeapPushDown(const LCL_type * lcl, const int ptr){
	return isMinLevel(ptr,lcl) ?
		MinMaxHeapPushDown(lcl,ptr,std::less<volatile int>()) : 
		MinMaxHeapPushDown(lcl,ptr,std::greater<volatile int>());
}

template <typename Comparator>
void MinMaxHeapPushUp(const LCL_type * lcl, const int ptr, const Comparator comp){
	const int gp = ptr>>2;
	if (gp > 0 && comp(lcl->counters[ptr].count,lcl->counters[gp].count)){ // grandparent exists
		swapAndMaintainHashtable(&lcl->counters[ptr],&lcl->counters[gp],lcl);
		MinMaxHeapPushUp(lcl,gp,comp);
	}
}

void MinMaxHeapPushUp(const LCL_type * lcl, const int ptr){
	LCLCounter tmp;
	LCLCounter *cpt = nullptr, *parent = nullptr;
	cpt = &lcl->counters[ptr];
	const int par = ptr>>1;
	parent = &lcl->counters[par];
	if (ptr==1){ return; } // if root return
	if (isMinLevel(ptr,lcl)){
		if (cpt->count > parent->count){ // min level and curr > parent, swap 
			swapAndMaintainHashtable(cpt,parent,lcl);
			MinMaxHeapPushUp(lcl,par,std::greater<volatile int>());
		}
		else{
			MinMaxHeapPushUp(lcl,ptr,std::less<volatile int>());
		}
	}
	else{
		if (cpt->count < parent->count){ // max level and curr < parent, swap 
			swapAndMaintainHashtable(cpt,parent,lcl);
			MinMaxHeapPushUp(lcl,par,std::less<volatile int>());
		}
		else{
			MinMaxHeapPushUp(lcl,ptr,std::greater<volatile int>());
		}
	}
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

void LCL_Update(LCL_type * lcl, const LCLitem_t item, const LCLweight_t value)
{
	LCLCounter * hashptr;
	// find whether new item is already stored, if so store it and add one
	// update heap property if necessary

	lcl->counters->item=0; // mark data structure as 'dirty'
	const int hashval=(int) hash31(lcl->hasha, lcl->hashb,item) % lcl->hashsize;
	// Might need below if hashval is 0
	/*if (hashval == 0){
		hashval=1;
	}*/
	hashptr=lcl->hashtable[hashval];
	
	// compute the hash value of the item, and begin to look for it in 
	// the hash table

	while (hashptr) {
		if (hashptr->item==item) {
			hashptr->count+=value; // increment the count of the item
			#if MINMAXHEAP
			hashptr = MinMaxHeapPushDown(lcl,hashptr - lcl->counters);
			MinMaxHeapPushUp(lcl,hashptr - lcl->counters);
			// If we are incrementing a count of a node in the middle of the tree,
			// we need to push it down and then up to maintain the heap property
			#else
			MinHeapBubbleDown(lcl,hashptr - lcl->counters); // and fix up the heap
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
	#if MINMAXHEAP
	MinMaxHeapPushDown(lcl,1);
	#else
	MinHeapBubbleDown(lcl,1);
	#endif
	// restore heap property if needed
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

// Output for Delegation Space-Saving
void LCL_Output(LCL_type *lcl, const int thresh, std::vector<std::pair<uint32_t,uint32_t>> &v)
{
	const int size=lcl->size;
	#if MINMAXHEAP
	int curr_node;
	plf::stack<uint16_t> stack;
	// 2 and 3 are the max nodes with largest count
	if (lcl->counters[2].count >= thresh){
		stack.push(2);
	}
	if (lcl->counters[3].count >= thresh){
		stack.push(3);
	}
	while(!stack.empty()){
		curr_node=stack.top();
		stack.pop();
		if (isMaxLevel(curr_node,lcl)){
			if (hasGrandchildren(curr_node,size)){ 
				int gc = curr_node << 2;
				if (lcl->counters[gc].count >= thresh){
					stack.push(gc);
				}
				if (lcl->counters[gc+1].count >= thresh){
					stack.push(gc+1);
				}
				if (lcl->counters[gc+2].count >= thresh){
					stack.push(gc+2);
				}
				if (lcl->counters[gc+3].count >= thresh){
					stack.push(gc+3);
				}
			}
			else{
				if (hasChildren(curr_node,size)){
					int ch = curr_node << 1;
					if (lcl->counters[ch].count >= thresh){
						stack.push(ch);
					}
					if (lcl->counters[ch+1].count >= thresh){
						stack.push(ch+1);
					}
				}
			}
		}
		else{ // Node is on a min level
			if (hasGrandparent(curr_node, size)){
				int gpar = curr_node >> 2;
				if (lcl->counters[gpar].count >= thresh){
					stack.push(gpar);
				}
			}
		}
		v.push_back(std::make_pair((uint32_t)lcl->counters[curr_node].item,lcl->counters[curr_node].count));
	}
	#else
	for (int i=1;i<=size;++i){
		if (lcl->counters[i].count >= thresh){
			v.push_back(std::make_pair((uint32_t)lcl->counters[i].item,lcl->counters[i].count));
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
