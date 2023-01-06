// lossycount.h -- header file for Lossy Counting
// see Manku & Motwani, VLDB 2002 for details
// implementation by Graham Cormode, 2002,2003

#ifndef OWFREQUENT_h
#define OWFREQUENT_h

#ifndef OWMINMAXHEAP
#define OWMINMAXHEAP 0
#endif

#include <cstdint>
#include <vector>

/////////////////////////////////////////////////////////
using OWFweight_t = int32_t;
using OWFitem_t = int32_t;
////////////////////////////////////////////////////////


typedef struct owfcounter_t
{
  volatile OWFitem_t item = 0; // item identifier
  int hash = 0; // its hash value
  volatile OWFweight_t count = 0; // (upper bound on) count for the item
  int delta = 0; // delta since last sent to merging thread
  owfcounter_t *prev, *next; // pointers in doubly linked list for hashtable
} OWFCounter; 
// 32 bytes

#define OWF_HASHMULT 3  // how big to make the hashtable of elements:
  // multiply 1/eps by this amount
  // about 3 seems to work well

typedef struct OWF_type
{
  OWFweight_t error;
  int hasha, hashb, hashsize;
  int size;
  bool *modtable;
  OWFCounter *root;
  OWFCounter *counters;
  OWFCounter ** hashtable; // array of pointers to items in 'counters'
  int M;
  float beta;
  uint32_t N_i;
} OWF_type;

OWF_type * OWF_Init(const double fPhi, const double beta);
void OWF_Destroy(OWF_type *);
void OWF_Update(OWF_type *, OWFitem_t, OWFweight_t);
void OWF_Update_Merging_Thread(OWF_type *, OWFitem_t, OWFweight_t);
int OWF_Size(OWF_type *);
OWFweight_t OWF_PointEst(OWF_type *, OWFitem_t);
OWFweight_t OWF_PointErr(OWF_type *, OWFitem_t);
void OWF_Output(OWF_type *,int,std::vector<std::pair<uint32_t,uint32_t>>&);
void OWF_ShowHeap(OWF_type *);
OWF_type * OWF_Copy(OWF_type *);
void OWF_CheckHash(OWF_type * lcl, uint32_t item, int hash);

#endif
