
#include <stdio.h>
#include <unordered_set>
#include "owfrequent.h"
#include "prng.h"
#include "buffer.h"
//#include "plf_stack.h"

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

constexpr OWFweight_t OWF_NULLITEM = 0x7FFFFFFF;
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

static inline bool isMinLevel(const int ptr, const OWF_type *owf) {
	return (owf->modtable[integerLog2(ptr)]);
	// even levels are min, odd ones are max
}

static inline bool isMaxLevel(const int ptr, const OWF_type *owf) {
	return isMinLevel(ptr,owf)^1;
}

template <typename Comparator>
static inline int minMaxChild(const OWF_type* owf, const int ptr, const Comparator comp) {
	int ch = ptr<<1;
	return(ch)+
			(comp(owf->counters[ch].count,owf->counters[ch+1].count) ? 0 : 1);
}

template <typename Comparator>
static inline int minMaxGrandchild(const OWF_type* owf, const int ptr, const Comparator comp) {
	int gc = ptr<<2;
	int c1 = gc +
			(comp(owf->counters[gc].count,owf->counters[gc+1].count) ? 0 : 1);
	int c2 = gc +
			(comp(owf->counters[gc+2].count,owf->counters[gc+3].count) ? 2 : 3);
	return (comp(owf->counters[c1].count,owf->counters[c2].count)) ? c1 : c2;
}

static inline bool hasGrandchildren(const int index, const int size) {
	return size >= ((index << 2) + 3);
}

static inline bool hasChildren(const int index, const int size) {
	return size >= ((index << 1) + 1);
}

static inline bool hasGrandparent(const int index) {
	return (index >> 2) > 0;
}

static inline void swapAndMaintainHashtable(OWFCounter *one, OWFCounter *other, const OWF_type *owf){
	OWFCounter tmp;

	tmp=*one;
	*one=*other;
	*other=tmp;
	// else, swap the parent and child in the heap

	if (one->hash==other->hash)
		// If both nodes are in the same linked list, make sure to 
		// completely swap the next and prev pointers of each node. 
	{ 
		other->prev=one->prev;
		one->prev=tmp.prev;
		other->next=one->next;
		one->next=tmp.next;
	} 
	
	else { // If the swapped nodes are in different linked lists,
		// make sure that each node is slotted into the linked list correctly
		if (!one->prev) { // if there is no previous pointer
			if (one->item!=OWF_NULLITEM)
				owf->hashtable[one->hash]=one; // put in pointer from hashtable
		} else
			one->prev->next=one; // Connect this one to the previous one
		if (one->next) 
			one->next->prev=one; // Connect this one to the next one

		//Do same as above for other node:
		if (!other->prev){
			if (other->item!=OWF_NULLITEM)
				owf->hashtable[other->hash]=other;
		}
		else
			other->prev->next=other; 
		if (other->next)
			other->next->prev=other;
	}
}

// *********** Implementation ***********

OWF_type * OWF_Init(const double fPhi, const double beta)
{
	int i;
	int k = 1 + (int) 1.0/fPhi;

	OWF_type *result = (OWF_type *) calloc(1,sizeof(OWF_type));
	#if OWMINMAXHEAP
	result->size = (1 + k) | 3; // ensure a node has either all granchildren or no grandchild
	#else
	result->size = (1 + k) | 1; // ensure that a node either all children or no child
	#endif

	result->beta = beta;
	result->N_i = 0;

	result->hashsize = OWF_HASHMULT*result->size;
	result->hashtable=(OWFCounter **) calloc(result->hashsize,sizeof(OWFCounter*));
	result->counters=(OWFCounter*) calloc(1+result->size,sizeof(OWFCounter)); // indexed from 1, so add 1


	result->hasha=151261303;
	result->hashb=6722461; // hard coded constants for the hash table,
	//should really generate these randomly
	result->error=(OWFweight_t) 0;

	for (i=1; i<=result->size;i++)
	{
		result->counters[i].next=NULL;
		result->counters[i].prev=NULL;
		result->counters[i].item=OWF_NULLITEM;
		result->counters[i].delta=0;
		// initialize items and counters to zero
	}

	result->modtable = (bool*) calloc(integerLog2(result->size)+1,sizeof(bool));
	for (i=0; i<(integerLog2(result->size)+1); i+=2)
		result->modtable[i] = 1;
	// initialize the modulo lookup table (1 on even levels, 0 on odd levels))

	result->root=&result->counters[1]; // put in a pointer to the top of the heap
	return result;
}

void OWF_Destroy(OWF_type * owf)
{
	free(owf->hashtable);
	free(owf->counters);
	free(owf->modtable);
	free(owf);
}

void OWF_RebuildHash(OWF_type * owf)
{
	// rebuild the hash table and linked list pointers based on current
	// contents of the counters array
	int i;
	OWFCounter * pt;

	for (i=0; i<owf->hashsize;i++)
		owf->hashtable[i]=0;
	// first, reset the hash table
	for (i=1; i<=owf->size;i++) {
		owf->counters[i].next=NULL;
		owf->counters[i].prev=NULL;
	}
	// empty out the linked list
	for (i=1; i<=owf->size;i++) { // for each item in the data structure
		pt=&owf->counters[i];
		pt->next=owf->hashtable[owf->counters[i].hash];
		if (pt->next)
			pt->next->prev=pt;
		owf->hashtable[owf->counters[i].hash]=pt;
	}
}

void inline MinHeapBubbleDown(const OWF_type * owf, int ptr)
{ // restore the heap condition in case it has been violated
	OWFCounter *cpt = nullptr, *minchild = nullptr;
	int mc = -1;

	while(1)
	{
		if (!hasChildren(ptr,owf->size)) break;
		// if the current node has no children

		mc = minMaxChild(owf, ptr, std::less<volatile OWFweight_t>());
		cpt=&owf->counters[ptr]; // create a current pointer
		minchild=&owf->counters[mc]; // create a pointer to the child with the smallest count

		if (cpt->count < minchild->count) break;
		// if the parent is less than the smallest child, we can stop

		swapAndMaintainHashtable(cpt,minchild,owf);
		ptr=mc;
		// continue on bubbling down from the child position
	}
}

template <typename Comparator>
OWFCounter* MinMaxHeapPushDown(const OWF_type *owf, const int ptr, const Comparator comp){
	OWFCounter *cpt,*mchild,*mgchild;
	cpt = mchild = mgchild = nullptr;
	int mc = -1, mgc = -1;
	cpt=&(owf->counters[ptr]);

	if (!hasChildren(ptr,owf->size)) return cpt; // if no children, return

	if (hasGrandchildren(ptr,owf->size)) { // if grandchild exists
		mgc = minMaxGrandchild(owf,ptr,comp);
		mgchild=&(owf->counters[mgc]);
	}

	mc = minMaxChild(owf,ptr,comp);
	mchild=&(owf->counters[mc]);

	if (mgchild && comp(mgchild->count,mchild->count)){ // (min/max)-grandchild exists, and is smaller/larger than smallest child
		if (comp(mgchild->count,cpt->count)){ // if (min/max)-grandchild is smaller/larger than current pointer
			swapAndMaintainHashtable(cpt,mgchild,owf); // swap pointers
			if (!comp(mgchild->count,owf->counters[mgc >> 1].count)){ // if (min/max)-grandchild is larger/smaller than its parent
				swapAndMaintainHashtable(&(owf->counters[mgc >> 1]),mgchild,owf); // swap with parent
			}
			return MinMaxHeapPushDown(owf,mgc,comp); // push down grandchild
		}
	}
	else if (comp(mchild->count,cpt->count)){ // if (min/max)-child is smaller/larger than current pointer
		swapAndMaintainHashtable(cpt,mchild,owf); // swap pointers
	}
	return cpt;
}

OWFCounter* MinMaxHeapPushDown(const OWF_type * owf, const int ptr){
	return isMinLevel(ptr,owf) ?
		MinMaxHeapPushDown(owf,ptr,std::less<volatile OWFweight_t>()) : 
		MinMaxHeapPushDown(owf,ptr,std::greater<volatile OWFweight_t>());
}

template <typename Comparator>
void MinMaxHeapPushUp(const OWF_type * owf, const int ptr, const Comparator comp){
	const int gp = ptr>>2;
	if (hasGrandparent(ptr) && comp(owf->counters[ptr].count,owf->counters[gp].count)){ // grandparent exists
		swapAndMaintainHashtable(&owf->counters[ptr],&owf->counters[gp],owf);
		MinMaxHeapPushUp(owf,gp,comp);
	}
}

void MinMaxHeapPushUp(const OWF_type * owf, const int ptr){
	if (ptr == 1){ return; } // if root return
	
	OWFCounter tmp;
	OWFCounter *cpt = nullptr, *parent = nullptr;
	cpt = &owf->counters[ptr];
	const int par = ptr >> 1;
	parent = &owf->counters[par];

	if (isMinLevel(ptr,owf)){
		if (cpt->count > parent->count){ // min level and curr > parent, swap 
			swapAndMaintainHashtable(cpt,parent,owf);
			MinMaxHeapPushUp(owf,par,std::greater<volatile OWFweight_t>());
		}
		else{
			MinMaxHeapPushUp(owf,ptr,std::less<volatile OWFweight_t>());
		}
	}
	else{
		if (cpt->count < parent->count){ // max level and curr < parent, swap 
			swapAndMaintainHashtable(cpt,parent,owf);
			MinMaxHeapPushUp(owf,par,std::less<volatile OWFweight_t>());
		}
		else{
			MinMaxHeapPushUp(owf,ptr,std::greater<volatile OWFweight_t>());
		}
	}
}

OWFCounter * OWF_FindItem(OWF_type * owf, OWFitem_t item)
{ // find a particular item in the date structure and return a pointer to it
	OWFCounter * hashptr;

	const int hashval=(int) hash31(owf->hasha, owf->hashb,item) % owf->hashsize;
	hashptr=owf->hashtable[hashval];
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

void OWF_Update(OWF_type * owf, const OWFitem_t item, const OWFweight_t value)
{
	OWFCounter * hashptr;
	// find whether new item is already stored, if so store it and add one
	// update heap property if necessary

	owf->counters->item=0; // mark data structure as 'dirty'
	const int hashval=(int) hash31(owf->hasha, owf->hashb,item) % owf->hashsize;
	hashptr=owf->hashtable[hashval];
	owf->N_i += value;
	
	// compute the hash value of the item, and begin to look for it in 
	// the hash table

	while (hashptr) {
		if (hashptr->item==item) {
			hashptr->count+=value; // increment the count of the item
			hashptr->delta+=value; // increment the frequency increment field
			if (hashptr->delta >= (int)(owf->beta*owf->N_i)){ 
				auto pair = std::make_pair(hashptr->item,hashptr->delta);
				putitem(pair); // put item in shared buffer
				hashptr->delta=0;
			}
			#if OWMINMAXHEAP
			hashptr = MinMaxHeapPushDown(owf,hashptr - owf->counters);
			MinMaxHeapPushUp(owf,hashptr - owf->counters);
			// If we are incrementing a count of a node in the middle of the tree,
			// we need to push it down and then up to maintain the heap property
			#else
			MinHeapBubbleDown(owf,hashptr - owf->counters); // and fix up the heap
			#endif
			return;
		}
		else hashptr=hashptr->next;
	}
	int cmin = owf->root->count - owf->M;
	if ((int)value < cmin){
		owf->M+=value;
	}
	else{
		owf->M+=cmin;
		// if control reaches here, then we have failed to find the item
		// fix up linked list from hashtable
		if (!owf->root->prev) // if it is first in its list
			owf->hashtable[owf->root->hash]=owf->root->next;
		else
			owf->root->prev->next=owf->root->next;
		if (owf->root->next) // if it is not last in the list
			owf->root->next->prev=owf->root->prev;
		// slot new item into hashtable
		hashptr=owf->hashtable[hashval];
		owf->root->next=hashptr;
		if (hashptr)
			hashptr->prev=owf->root;
		owf->hashtable[hashval]=owf->root;
		// we overwrite the smallest item stored, so we look in the root
		owf->root->prev=NULL;
		owf->root->item=item;
		owf->root->hash=hashval;
		// update the implicit lower bound on the items frequency
		//  value+=owf->root->delta;
		// update the upper bound on the items frequency
		owf->root->count+=value - cmin;
		owf->root->delta=value;

		#if OWMINMAXHEAP
		MinMaxHeapPushDown(owf,1);
		#else
		MinHeapBubbleDown(owf,1);
		#endif
		// restore heap property if needed
		if (owf->root->delta >= (int)(owf->beta*owf->N_i)){ 
			auto pair = std::make_pair(owf->root->item,owf->root->delta);
			putitem(pair); // put item in shared buffer
			owf->root->delta=0;
		}
	}
	// Send a message to the merging thread if frequency increment is above threshold
}

void OWF_Update_Merging_Thread(OWF_type * owf, const OWFitem_t item, const OWFweight_t value)
{
	OWFCounter * hashptr;
	// find whether new item is already stored, if so store it and add one
	// update heap property if necessary

	owf->counters->item=0; // mark data structure as 'dirty'
	const int hashval=(int) hash31(owf->hasha, owf->hashb,item) % owf->hashsize;
	hashptr=owf->hashtable[hashval];
	
	// compute the hash value of the item, and begin to look for it in 
	// the hash table

	while (hashptr) {
		if (hashptr->item==item) {
			hashptr->count+=value; // increment the count of the item
			#if OWMINMAXHEAP
			hashptr = MinMaxHeapPushDown(owf,hashptr - owf->counters);
			MinMaxHeapPushUp(owf,hashptr - owf->counters);
			// If we are incrementing a count of a node in the middle of the tree,
			// we need to push it down and then up to maintain the heap property
			#else
			MinHeapBubbleDown(owf,hashptr - owf->counters); // and fix up the heap
			#endif
			return;
		}
		else hashptr=hashptr->next;
	}
	int cmin = owf->root->count - owf->M;
	if ((int)value < cmin){
		owf->M+=value;
	}
	else{
		owf->M+=cmin;
		// if control reaches here, then we have failed to find the item
		// fix up linked list from hashtable
		if (!owf->root->prev) // if it is first in its list
			owf->hashtable[owf->root->hash]=owf->root->next;
		else
			owf->root->prev->next=owf->root->next;
		if (owf->root->next) // if it is not last in the list
		owf->root->next->prev=owf->root->prev;

		// slot new item into hashtable
		hashptr=owf->hashtable[hashval];
		owf->root->next=hashptr;
		if (hashptr)
			hashptr->prev=owf->root;
		owf->hashtable[hashval]=owf->root;
		// we overwrite the smallest item stored, so we look in the root
		owf->root->prev=NULL;
		owf->root->item=item;
		owf->root->hash=hashval;
		// update the implicit lower bound on the items frequency
		//  value+=owf->root->delta;
		// update the upper bound on the items frequency
		owf->root->count+=value - cmin;
		#if OWMINMAXHEAP
		MinMaxHeapPushDown(owf,1);
		#else
		MinHeapBubbleDown(owf,1);
		#endif
		// restore heap property if needed
	}
}

int OWF_Size(OWF_type * owf)
{ // return the size of the data structure in bytes
	return sizeof(OWF_type) + (owf->hashsize * sizeof(int)) + 
		(owf->size * sizeof(int)) + // size of maxheap 
		(owf->size*sizeof(OWFCounter));
}

OWFweight_t OWF_PointEst(OWF_type * owf, OWFitem_t item)
{ // estimate the count of a particular item
	OWFCounter * i;
	i=OWF_FindItem(owf,item);
	if (i)
		return(i->count);
	else
		return 0;
}

OWFweight_t OWF_PointErr(OWF_type * owf, OWFitem_t item)
{ // estimate the worst case error in the estimate of a particular item
 // Modified, this implementation does not track per-element error
	return owf->error;
}

int OWF_cmp( const void * a, const void * b) {
	OWFCounter * x = (OWFCounter*) a;
	OWFCounter * y = (OWFCounter*) b;
	if (x->count<y->count) return -1;
	else if (x->count>y->count) return 1;
	else return 0;
}

// Output for Delegation Space-Saving
void OWF_Output(OWF_type *owf, const int thresh, std::vector<std::pair<uint32_t,uint32_t>> &v)
{
	const int size=owf->size;
	#if OWMINMAXHEAP
	uint16_t curr_node;
	std::stack<uint16_t> stack;
	std::unordered_set <uint16_t> visitedgpars;
	// 2 and 3 are the max nodes with largest count
	if (owf->counters[2].count>thresh)
		stack.push(2);
	if (owf->counters[3].count>thresh)
		stack.push(3);
	while(!stack.empty()){
		curr_node = stack.top();
		stack.pop();
		if (isMaxLevel(curr_node,owf)){
			if (hasGrandchildren(curr_node,size)){
				uint16_t gc = curr_node << 2;
				if (owf->counters[gc].count >= thresh)
					stack.push(gc);
				if (owf->counters[gc+1].count >= thresh)
					stack.push(gc+1);
				if (owf->counters[gc+2].count >= thresh)
					stack.push(gc+2);
				if (owf->counters[gc+3].count >= thresh)
					stack.push(gc+3);
			}
			else{
				if (hasChildren(curr_node,size)){
					uint16_t ch = curr_node << 1;
					if (owf->counters[ch].count >= thresh)
						stack.push(ch);
					if (owf->counters[ch+1].count >= thresh)
						stack.push(ch+1);
				}
			}
		}	
		else{ // Node is on a min level
			if (hasGrandparent(curr_node)){
				uint16_t gpar = curr_node >> 2;
				if (visitedgpars.find(gpar) == visitedgpars.end()){
					if (owf->counters[gpar].count >= thresh){
						stack.push(gpar);
						visitedgpars.insert(gpar);
					}
				}
			}
		}
		v.push_back(std::make_pair(owf->counters[curr_node].item,owf->counters[curr_node].count));
	}
	#else
	for (int i=1;i <=size;++i){
		if (owf->counters[i].count - owf->M >= thresh){
			v.push_back(std::make_pair(owf->counters[i].item,owf->counters[i].count - owf->M));
		}
	}
	#endif
}

void OWF_ShowHeap(OWF_type * owf)
{ // debugging routine to show the heap
	int i, j;
	int level=0;
	printf("Min-heap tree:\n");
	printf("level:%d\n",level);
	j=1;
	for (i=1; i<=owf->size; i++)
	{
		printf("%u ",(int) owf->counters[i].count);
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
	for (i=1; i<=owf->size; i++){
		printf("%u ",owf->counters[i].item);
	}
	printf("\nCount: ");
	for (i=1; i<=owf->size; i++){
		printf("%u ",owf->counters[i].count);
	}
	printf("\n");
}
