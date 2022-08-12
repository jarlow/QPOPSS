// lossycount.h -- header file for Lossy Counting
// see Manku & Motwani, VLDB 2002 for details
// implementation by Graham Cormode, 2002,2003

#ifndef LOSSYCOUNTING_h
#define LOSSYCOUNTING_h

#ifndef MAXHEAP
#define MAXHEAP 0
#endif

#include "prng.h"

// lclazy.h -- header file for Lazy Lossy Counting
// see Manku & Motwani, VLDB 2002 for details
// implementation by Graham Cormode, 2002,2003, 2005

/////////////////////////////////////////////////////////
#define LCLweight_t int
//#define LCL_SIZE 101 // size of k, for the summary
// if not defined, then it is dynamically allocated based on user parameter
////////////////////////////////////////////////////////

#define LCLitem_t uint32_t

typedef struct lclcounter_t LCLCounter;

struct lclcounter_t
{
  volatile LCLitem_t item=0; // item identifier
  int hash = 0; // its hash value
  volatile LCLweight_t count = 0; // (upper bound on) count for the item
  LCLCounter *prev, *next; // pointers in doubly linked list for hashtable
  LCLCounter** maxheapptr;
}; // 32 bytes

#define LCL_HASHMULT 3  // how big to make the hashtable of elements:
  // multiply 1/eps by this amount
  // about 3 seems to work well

#ifdef LCL_SIZE
#define LCL_SPACE (LCL_HASHMULT*LCL_SIZE)
#endif

typedef struct LCL_type
{
  
  LCLweight_t error;
  int hasha, hashb, hashsize;
  int size;
  LCLCounter *root;

#ifdef LCL_SIZE
  LCLCounter counters[LCL_SIZE+1]; // index from 1
  LCLCounter *maxheap[LCL_SIZE+1]; // index from 1
  LCLCounter *hashtable[LCL_SPACE]; // array of pointers to items in 'counters'
  // 24 + LCL_SIZE*(32 + LCL_HASHMULT*4) + 8
            // = 24 + 102*(32+12)*4 = 4504
            // call it 4520 for luck...
#else
  LCLCounter *counters;
  LCLCounter **maxheap;
  LCLCounter ** hashtable; // array of pointers to items in 'counters'
#endif
} LCL_type;

extern LCL_type * LCL_Init(float fPhi);
extern void LCL_Destroy(LCL_type *);
extern void LCL_Update(LCL_type *, LCLitem_t, int);
extern int LCL_Size(LCL_type *);
extern int LCL_PointEst(LCL_type *, LCLitem_t);
extern int LCL_PointErr(LCL_type *, LCLitem_t);
extern int LCL_Output(LCL_type *,int,uint32_t*,uint32_t*);
extern void LCL_Output(LCL_type *,int,std::vector<std::pair<uint32_t,uint32_t>>*);
extern int LCL_Output(LCL_type *,int,std::pair<uint32_t,uint32_t>*);
extern std::vector<std::pair<uint32_t,uint32_t>> LCL_Output(LCL_type *,int);
extern void LCL_Output(LCL_type*);
extern void LCL_ShowHash(LCL_type *);
extern void LCL_ShowHeap(LCL_type *);
extern void LCL_ShowHeapMax(LCL_type *);
extern LCL_type * LCL_Copy(LCL_type *);

#endif
