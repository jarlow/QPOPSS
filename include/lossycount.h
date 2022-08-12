// lossycount.h -- header file for Lossy Counting
// see Manku & Motwani, VLDB 2002 for details
// implementation by Graham Cormode, 2002,2003

#ifndef LOSSYCOUNTING_h
#define LOSSYCOUNTING_h

#ifndef MINMAXHEAP
#define MINMAXHEAP 0
#endif

#include "prng.h"

// lclazy.h -- header file for Lazy Lossy Counting
// see Manku & Motwani, VLDB 2002 for details
// implementation by Graham Cormode, 2002,2003, 2005

/////////////////////////////////////////////////////////
using LCLweight_t = int;
using LCLitem_t = uint32_t;
////////////////////////////////////////////////////////


typedef struct lclcounter_t
{
  volatile LCLitem_t item = 0; // item identifier
  int hash = 0; // its hash value
  volatile LCLweight_t count = 0; // (upper bound on) count for the item
  //int delta = 0; // track per-element error
  lclcounter_t *prev, *next; // pointers in doubly linked list for hashtable
} LCLCounter; 
// 28 bytes

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
  LCLCounter *counters;
  LCLCounter ** hashtable; // array of pointers to items in 'counters'
} LCL_type;

LCL_type * LCL_Init(float fPhi);
void LCL_Destroy(LCL_type *);
void LCL_Update(LCL_type *, LCLitem_t, int);
int LCL_Size(LCL_type *);
int LCL_PointEst(LCL_type *, LCLitem_t);
int LCL_PointErr(LCL_type *, LCLitem_t);
void LCL_Output(LCL_type *,int,std::vector<std::pair<uint32_t,uint32_t>>&);
void LCL_ShowHeap(LCL_type *);
LCL_type * LCL_Copy(LCL_type *);

#endif
