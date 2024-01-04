// lossycount.h -- header file for Lossy Counting
// see Manku & Motwani, VLDB 2002 for details
// implementation by Graham Cormode, 2002,2003

#ifndef LOSSYCOUNTING_h
#define LOSSYCOUNTING_h

#ifndef MAXHEAP
#define MAXHEAP 0
#endif

#include "prng.h"

#include <cstdint>
#include <vector>

/////////////////////////////////////////////////////////
using LCLweight_t = uint32_t;
using LCLitem_t = uint32_t;
////////////////////////////////////////////////////////


typedef struct lclcounter_t
{
  volatile LCLitem_t item = 0; // item identifier
  int hash = 0; // its hash value
  volatile LCLweight_t count = 0; // (upper bound on) count for the item
  LCLweight_t delta = 0; // max possible error in count for the value
  LCLCounter *prev, *next; // pointers in doubly linked list for hashtable
  LCLCounter **maxheapptr;
} LCLCounter; // 32 bytes

#define LCL_HASHMULT 3  // how big to make the hashtable of elements:
  // multiply 1/eps by this amount
  // about 3 seems to work well

typedef struct LCL_type
{
  LCLweight_t error;
  int hasha, hashb, hashsize;
  int size;
  bool *modtable;
  LCLCounter *root;
#ifdef LCL_SIZE
  LCLCounter counters[LCL_SIZE+1]; // index from 1
  LCLCounter maxheap[LCL_SIZE+1]; // index from 1
  LCLCounter *hashtable[LCL_SPACE]; // array of pointers to items in 'counters'
  // 24 + LCL_SIZE*(32 + LCL_HASHMULT*4) + 8
            // = 24 + 102*(32+12)*4 = 4504
            // call it 4520 for luck...
#else
  alignas(64) LCLCounter *counters;
  alignas(64) LCLCounter **maxheap;
  LCLCounter ** hashtable; // array of pointers to items in 'counters'
} LCL_type;

LCL_type * LCL_Init(float fPhi);
void LCL_Destroy(LCL_type *);
void LCL_Update(LCL_type *, LCLitem_t, LCLweight_t);
int LCL_Size(LCL_type *);
LCLweight_t LCL_PointEst(LCL_type *, LCLitem_t);
LCLweight_t LCL_PointErr(LCL_type *, LCLitem_t);
int LCL_Output(LCL_type *,int,std::vector<std::pair<uint32_t,uint32_t>>*);
void LCL_ShowHeap(LCL_type *);
LCL_type * LCL_Copy(LCL_type *);
void LCL_CheckHash(LCL_type * lcl, uint32_t item, int hash);

#endif
