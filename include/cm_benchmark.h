#ifndef CM_BENCHMARK_H
#define CM_BENCHMARK_H

#include "thread_data.h"

//define preprocessor flags that are not set in the Makefile
#ifndef SHARED_SKETCH
#define SHARED_SKETCH 0
#endif

#ifndef LOCAL_COPIES
#define LOCAL_COPIES 0
#endif

#ifndef HYBRID
#define HYBRID 0
#endif

#ifndef REMOTE_INSERTS
#define REMOTE_INSERTS 0
#endif

#ifndef USE_MPSC
#define USE_MPSC 0
#endif

#ifndef USE_FILTER
#define USE_FILTER 0
#endif

#ifndef AUGMENTED_SKETCH
#define AUGMENTED_SKETCH 0
#endif

#ifndef DELEGATION_FILTERS
#define DELEGATION_FILTERS 0
#endif

#ifndef USE_LIST_OF_FILTERS
#define USE_LIST_OF_FILTERS 0
#endif

#ifndef FREQUENT
#define FREQUENT 0
#endif

#ifndef SPACESAVING
#define SPACESAVING 0
#endif

#ifndef FILTERONES
#define FILTERONES 0
#endif

#ifndef SINGLE
#define SINGLE 0
#endif

#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef ACCURACY
#define ACCURACY 0
#endif

#ifndef TOPKAPI
#define TOPKAPI 0
#endif

#ifndef ITHACA
#define ITHACA 0
#endif

#ifndef GENERATE_MODE
#define GENERATE_MODE 0
#endif

#define PREINSERT 1
#define LATENCY 1

int QUERRY_RATE;
int TOPK_QUERY_RATE;
int DURATION;
int COUNTING_PARAM;

void * threadEntryPoint(void * threadArgs);
void insert(threadDataStruct * localThreadData, unsigned int key, unsigned int increment);
#endif