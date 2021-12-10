#include "relation.h"
#include "xis.h"
#include "sketches.h"
#include "utils.h"
#include <utility>
#include "thread_utils.h"
#include "filter.h"
#include "getticks.h"
#include "lossycount.h"
#include "LossyCountMinSketch.h"

#include <numeric> 
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <x86intrin.h>
#include <algorithm>
#include <random>
#include <string.h>
#include <unordered_map>
#include <set>

#define PREINSERT 0
#define NO_SQUASHING 0
#define HASHA 151261303
#define HASHB 6722461
#define TRUE 1
#define FALSE 0

using namespace std;

FilterStruct * filterMatrix;

set<uint32_t> uniques;
uint32_t uniques_no;
int K;
float PHI;
int MAX_FILTER_SUM,MAX_FILTER_UNIQUES;
int tuples_no,rows_no;


/* TOPKAPI */

void allocate_sketch( LossySketch* sketch,
                      const unsigned range)
{
  int i;
  (*sketch)._b           = range;
  (*sketch).identity     = (uint32_t*) malloc(range*sizeof(uint32_t));
  (*sketch).lossyCount   = (int* ) malloc(range*sizeof(int));
	if ( (*sketch).identity == NULL ||
			 (*sketch).lossyCount == NULL )
	{
		fprintf(stderr, "LossySketch allocation error!\n");
		exit(EXIT_FAILURE);
	}
  /* set counts to -1 to indicate empty counter */
  for (i = 0; i < range; ++i){
    (*sketch).lossyCount[i] = -1;
    (*sketch).identity[i] = 0;
  }
}

/* Frees a row of topkapi sketch data structure */
void deallocate_sketch( LossySketch* sketch )
{
  free((*sketch).identity);
  free((*sketch).lossyCount);
}

/* TOPKAPI */ 


unsigned short precomputedMods[512];

static inline int findOwner(unsigned int key){
    //return precomputedMods[hash31(HASHA,HASHB,key) & (BUCKETS-1)]; // Cardinality estimation
    return precomputedMods[key & 511];
}
volatile int threadsFinished = 0;
//#if ((!DURATION && DELEGATION_FILTERS) || (PREINSERT))
//volatile int threadsFinished = 0;
//#endif

unsigned int Random_Generate()
{
    unsigned int x = rand();
    unsigned int h = rand();

    return x ^ ((h & 1) << 31);
}


static inline unsigned long* seed_rand()
{
    unsigned long* seeds;
    /* seeds = (unsigned long*) ssalloc_aligned(64, 64); */
    //seeds = (unsigned long*) memalign(64, 64);
    seeds = (unsigned long*) calloc(3,64);
    seeds[0] = getticks() % 123456789;
    seeds[1] = getticks() % 362436069;
    seeds[2] = getticks() % 521288629;
    return seeds;
}

#define my_random xorshf96

static inline unsigned long
xorshf96(unsigned long* x, unsigned long* y, unsigned long* z)  //period 2^96-1
{
    unsigned long t;
    (*x) ^= (*x) << 16;
    (*x) ^= (*x) >> 5;
    (*x) ^= (*x) << 1;

    t = *x;
    (*x) = *y;
    (*y) = *z;
    (*z) = t ^ (*x) ^ (*y);

  return *z;
}
int shouldQuery(threadDataStruct *ltd){
    return (my_random(&(ltd->seeds[0]), &(ltd->seeds[1]), &(ltd->seeds[2])) % 1000);
}
int shouldTopKQuery(threadDataStruct *ltd){
    return ((my_random(&(ltd->seeds[0]), &(ltd->seeds[2]), &(ltd->seeds[1]))) % 1000000);
}

/* Used for cardinality estimation */
void updateBucket(threadDataStruct * localThreadData,uint32_t key){
    // Count leading zeros of the key
    uint32_t hash=hash31(HASHA,HASHB,key);
    int num_zeros=__builtin_clz(hash);
    int thr_local_idx=(hash & (BUCKETS-1))/numberOfThreads;
    /*update the bucket if leading zeros of key exceed previous max*/
    if (num_zeros > localThreadData->buckets[thr_local_idx]){
        if (localThreadData->buckets[thr_local_idx] != 0){
            /* remove old outdated value */
            localThreadData->sum_of_buckets-=pow(2.0,-localThreadData->buckets[thr_local_idx]);
        }
        /* add new value to the running total */
        localThreadData->buckets[thr_local_idx]=num_zeros;
        localThreadData->sum_of_buckets+=pow(2.0,-num_zeros);
    } 
}

void serveDelegatedInserts(threadDataStruct * localThreadData){
    // Check if there is a full filter that needs emptying
    if (!localThreadData->listOfFullFilters) return;
    //take trylock or return from this function
    if(pthread_mutex_trylock(&localThreadData->mutex)){
        return;
    }
    while (localThreadData->listOfFullFilters){
        // Select first filter in queue
        FilterStruct* filter = pop(&(localThreadData->listOfFullFilters));
        #if DEBUG 
        // If debug keep track of number of filters and sum inserted per thread
        localThreadData->numInsertedFilters++;
        localThreadData->accumFilters+=filter->filterSum;
        for (int i=0; i<MAX_FILTER_UNIQUES;i++){
            // Keep track of true number of uniques inserted
            if (!localThreadData->uniques->count(filter->filter_id[i])){
                localThreadData->num_uniques++;
                localThreadData->uniques->insert(filter->filter_id[i]);
            }
        }
        #endif
        // parse filter and add each element to your own filter
        for (int i=0; i<filter->filterCount;i++){
            unsigned int count = filter->filter_count[i];
            int key = filter->filter_id[i];
            #if SPACESAVING
            LCL_Update(localThreadData->ss,key,count);
            //updateBucket(localThreadData,key); // Cardinality estimation
            #else // If vanilla Delegation Sketch is used
            insertFilterNoWriteBack(localThreadData, key, count);
            #endif
            // flush each element
            filter->filter_id[i] = -1;
            filter->filter_count[i] = 0;
        }
        // mark filter as empty
        filter->filterCount = 0;
        filter->filterSum = 0;
        filter->filterFull = 0;
    }
    // release mutex lock
    pthread_mutex_unlock(&localThreadData->mutex);
}

unsigned int queryAllRelatedDataStructuresAndUpdatePendigQueries(threadDataStruct * localThreadData, unsigned int key){
    unsigned int countInFilter = queryFilter(key, &(localThreadData->Filter));
    unsigned int queryResult = 0;
    if (countInFilter){
        queryResult = countInFilter;
    }
    else{
        queryResult = localThreadData->theSketch->Query_Sketch(key);
    }
    //Also check the delegation filters
    for (int j =0; j < (numberOfThreads); j++){
        queryResult += queryFilter(key, &(filterMatrix[j * (numberOfThreads) + localThreadData->tid]));
    }
    
    #if !NO_SQUASHING
    // Search all the pending queries to find queries with the same key that can be served
    for (int j=0; j< (numberOfThreads); j++){
        if (localThreadData->pendingQueriesFlags[j] && (localThreadData->pendingQueriesKeys[j] == key)){
            localThreadData->pendingQueriesCounts[j] = queryResult;
            localThreadData->pendingQueriesFlags[j] = 0;
        }
    }
    #endif	
    return queryResult;
}

void serveDelegatedQueries(threadDataStruct *localThreadData){
    //if (!localThreadData->queriesPending) return;
    for (int i=0; i<(numberOfThreads); i++){
        if (localThreadData->pendingQueriesFlags[i]){
            int key = localThreadData->pendingQueriesKeys[i];
            unsigned int ret = queryAllRelatedDataStructuresAndUpdatePendigQueries(localThreadData, key);
	    #if NO_SQUASHING
            	localThreadData->pendingQueriesCounts[i] = ret;
            	localThreadData->pendingQueriesFlags[i] = 0;
	    #endif
        }
    }
    //localThreadData->queriesPending = 0;
}

void serveDelegatedInsertsAndQueries(threadDataStruct *localThreadData){
    serveDelegatedInserts(localThreadData);
    // Do not perform point-queries
    //serveDelegatedQueries(localThreadData); 
}


static inline void delegateInsert(threadDataStruct * localThreadData, unsigned int key, unsigned int increment, int owner){
    #if SINGLE
    LCL_Update(threadData[0].ss,key,1);
    localThreadData->counter++;
    updateBucket(localThreadData,key); // cardinality estimation
    return;
    #else
    /* Use cachecounter to prevent false-sharing */
    localThreadData->cachecounter++;
    if (localThreadData->cachecounter >= 100){
        localThreadData->counter+=100;
        localThreadData->cachecounter=0;    
    }
    #endif
    FilterStruct  * filter = &(filterMatrix[localThreadData->tid * (numberOfThreads) + owner]);
    #if USE_LIST_OF_FILTERS
    if (!startBenchmark){
        return;
    }
    // Uncomment for other approach, count the occurrence of elements locally instead of per filter.
    localThreadData->sumcounter++;
    while (filter->filterFull){
        serveDelegatedInsertsAndQueries(localThreadData);
    }
    tryInsertInDelegatingFilterWithListAndMaxSum(filter, key);
    /* If the current filter contains max uniques, or if the number of inserts since last window is equal to max, flush all filters */
    if (localThreadData->sumcounter == MAX_FILTER_SUM || filter->filterCount == MAX_FILTER_UNIQUES ){
    //if (filter->filterSum == MAX_FILTER_SUM || filter->filterCount == MAX_FILTER_UNIQUES ){ // old method
        /* push all filters to the other threads */
        for (int i=0;i< numberOfThreads;i++){
            filter = &(filterMatrix[localThreadData->tid * (numberOfThreads) + i]);
            filter->filterFull=1;
            threadDataStruct * owningThread = &(threadData[i]);
            push(filter, &(owningThread->listOfFullFilters));
        }
        /* Make sure all filters are empty before continuing */
        for (int i=0;i< numberOfThreads;i++){
            filter = &(filterMatrix[localThreadData->tid * (numberOfThreads) + i]);
            while( filter->filterFull  && startBenchmark){  
                serveDelegatedInsertsAndQueries(localThreadData);
            }
        }  
        localThreadData->sumcounter=0;     
    }

    #else
    while((!tryInsertInDelegatingFilter(filter, key)) && startBenchmark){   // I might deadlock if i am waiting for a thread that finished the benchmark
        //If it is full? Maybe try to serve your own pending requests and try again?
        if (!threadData[owner].insertsPending) threadData[owner].insertsPending = 1;
        serveDelegatedInsertsAndQueries(localThreadData);
    }
    #endif
}

unsigned int delegateQuery(threadDataStruct * localThreadData, unsigned int key){
    int owner = findOwner(key);

    if (owner == localThreadData->tid){
        unsigned int ret = queryAllRelatedDataStructuresAndUpdatePendigQueries(localThreadData, key);
        return ret;
    }

    threadData[owner].pendingQueriesKeys[localThreadData->tid] = key;
    threadData[owner].pendingQueriesCounts[localThreadData->tid] = 0;
    threadData[owner].pendingQueriesFlags[localThreadData->tid] = 1;

    while (threadData[owner].pendingQueriesFlags[localThreadData->tid] && startBenchmark){
        serveDelegatedInsertsAndQueries(localThreadData);
    }
    return threadData[owner].pendingQueriesCounts[localThreadData->tid];
}

double querry(threadDataStruct * localThreadData, unsigned int key){
    #if HYBRID
    double approximate_freq = localThreadData->theGlobalSketch->Query_Sketch(key);
    approximate_freq += (HYBRID-1)*numberOfThreads; //The amount of slack that can be hiden in the local copies
    #elif REMOTE_INSERTS || USE_MPSC
    double approximate_freq = localThreadData->sketchArray[findOwner(key)]->Query_Sketch(key);
    #elif LOCAL_COPIES
    double approximate_freq = 0;
    for (int j=0; j<numberOfThreads; j++){
        approximate_freq += localThreadData->sketchArray[j]->Query_Sketch(key);
    }
    #elif AUGMENTED_SKETCH   // WARNING: Queries are not thread safe right now
    double approximate_freq = 0;
    for (int j=0; j<numberOfThreads; j++){
        unsigned int countInFilter = queryFilter(key, &(threadData[j].Filter));
        if (countInFilter){
            approximate_freq += countInFilter;
        }
        else{
            approximate_freq += localThreadData->sketchArray[j]->Query_Sketch(key);
        }
    }
    #elif SHARED_SKETCH
    double approximate_freq = localThreadData->theGlobalSketch->Query_Sketch(key);
    #elif DELEGATION_FILTERS
    double approximate_freq = delegateQuery(localThreadData, key);
    #else 
    double approximate_freq = 0;
        //#error "Preprocessor flags not properly set"
    #endif
    #if USE_FILTER
    approximate_freq += (MAX_FILTER_SLACK-1)*numberOfThreads; //The amount of slack that can be hiden in the local copies
    #endif

    return approximate_freq;
}


void insert(threadDataStruct * localThreadData, unsigned int key, unsigned int increment){
#if TOPKAPI
    //localThreadData->topkapi_instance->Update_Sketch(key,increment);
    for (int i = 0; i < rows_no; ++i){
        update_sketch( &(localThreadData->th_local_sketch[localThreadData->tid * rows_no + i]), key, localThreadData->randoms,i );
    }
#elif REMOTE_INSERTS
    int owner = findOwner(key);
    localThreadData->sketchArray[owner]->Update_Sketch_Atomics(key, increment);
#elif HYBRID
    localThreadData->theSketch->Update_Sketch_Hybrid(key, 1.0, HYBRID);
#elif LOCAL_COPIES || AUGMENTED_SKETCH || DELEGATION_FILTERS
    localThreadData->theSketch->Update_Sketch(key, double(increment));
#elif SHARED_SKETCH
    localThreadData->theGlobalSketch->Update_Sketch_Atomics(key, increment);
#elif USE_MPSC
    int owner = findOwner(key); 
    localThreadData->sketchArray[owner]->enqueueRequest(key);
    localThreadData->theSketch->serveAllRequests(); //Serve any requests you can find in your own queue
#endif
}

bool sortbysecdesc(const pair<uint32_t,uint32_t> &a,
                   const pair<uint32_t,uint32_t> &b)
{
       return a.second>b.second;
}

void topkapi_query(threadDataStruct * localThreadData,int K,float phi,vector<pair<uint32_t,uint32_t>>* v ){
    v->clear();
    std::unordered_map<uint32_t,uint32_t> res;
    for (int t=0;t < numberOfThreads;t++){
        threadData[t].topkapi_instance->Query_Local_Sketch(&res);
    }
    for (auto elem : res){
        v->push_back(elem);
    }
    std::sort(v->data(), v->data()+v->size(), sortbysecdesc);

    /* slice away elements that are not part of the top k */
    v->erase(v->end()-(v->size()-K),v->end());
}

int fast_mod(const int input, const int ceil) {
    // apply the modulo operator only when needed
    // (i.e. when the input is greater than the ceiling)
    return input >= ceil ? input % ceil : input;
    // NB: the assumption here is that the numbers are positive
}


// Performs a frequent elements query 
void FEquery(threadDataStruct * localThreadData,int K,float phi,vector<pair<uint32_t,uint32_t>>* v ){
    //float cardinality_estimate=0.0;
    LCL_type* local_spacesaving;
    v->clear();
    uint64_t streamsize=0;
    
    // If single threaded just extract frequent elements from the local Space-Saving instance
    #if SINGLE
    streamsize = localThreadData->counter;
    local_spacesaving = localThreadData->ss;
    LCL_Output(local_spacesaving,streamsize*phi,v);
    //cardinality_estimate=threadData[0].sum_of_buckets;

    // If Delegation Space-Saving then extract local frequent elements at all threads
    #else

    // Estimate stream size across all threads
    for (int j=0;j<numberOfThreads;j++){
        streamsize += threadData[j].counter;
    }

    // Get all local frequent elements at the threads
    bool bm[numberOfThreads]={0};
    //uint32_t bm=0;
    int num_complete=0;
    int i=0;
    while (num_complete < numberOfThreads){
        //if(!((bm >> i) & 1)){ // check if i'th bit is 0
        if (!bm[i]){
            if(pthread_mutex_trylock(&threadData[i].mutex)){
                // try next
                i=precomputedMods[i+1];
                continue;
            }
            local_spacesaving=threadData[i].ss;
            LCL_Output(local_spacesaving,streamsize*phi,v);
            //cardinality_estimate+=threadData[i].sum_of_buckets;
            pthread_mutex_unlock(&threadData[i].mutex);
            bm[i]=true;
            //bm |= (1 << i); // set i'th bit to 1
            num_complete++;
            //serveDelegatedInserts(localThreadData);
        }
        /*else{
            i=__builtin_ctz((~bm & (bm+1))); // find position of rightmost unset bit
        }*/
        serveDelegatedInserts(localThreadData);
        i=precomputedMods[i+1];
    }
    #endif

    // Sort output
    std::sort(v->data(), v->data()+v->size(),sortbysecdesc);
    //cardinality_estimate=BUCKETS_SQ*0.709/cardinality_estimate;
    //return cardinality_estimate;
}

uint64_t rdtsc(){
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

void threadWork(threadDataStruct *localThreadData)
{
    int start = localThreadData->startIndex;
    int end = localThreadData->endIndex;
    int i;
    int numInserts = 0;
    int numQueries = 0;
    int numTopKQueries = 0;
    while (!startBenchmark)
    {
    }
    while (startBenchmark)
    {
        for (i = start; i < end; i++)
        {
            unsigned int key = (*localThreadData->theData->tuples)[i];
            if (!startBenchmark)
            {
                break;
            }
            // If we allow point-queries:
            /*
            if (shouldQuery(localThreadData) < QUERRY_RATE)
            {
                numQueries++;
                #if DELEGATION_FILTERS
                serveDelegatedQueries(localThreadData);
                #endif
                double approximate_freq = querry(localThreadData, key);
                localThreadData->returnData += approximate_freq;
            }*/
            if (shouldTopKQuery(localThreadData) < TOPK_QUERY_RATE)
            {
                #if LATENCY
                uint64_t tick = rdtsc();  // Latency measurement
                #endif

                #if SPACESAVING
                FEquery(localThreadData,K,PHI,&(localThreadData->lasttopk));
                #elif TOPKAPI
                //topkapi_query(localThreadData,K,PHI,&(localThreadData->lasttopk));
                #endif

                #if LATENCY
                localThreadData->latencies[numTopKQueries]=rdtsc() - tick;
                //printf("after: %lu\n",rdtsc() - tick); // Latency measurement
                #endif
                numTopKQueries++;
            }                        
            numInserts++;
            #if TOPKAPI
            insert(localThreadData, key, 1);
            #elif DELEGATION_FILTERS
            #if SINGLE
            delegateInsert(localThreadData, key, 1, 0);
            #else
            serveDelegatedInserts(localThreadData);
            // int old_owner = key - numberOfThreads * libdivide::libdivide_s32_do((uint32_t)key, fastDivHandle);
            int owner = findOwner(key);
            delegateInsert(localThreadData, key, 1, owner);
            #endif
            #elif AUGMENTED_SKETCH
            insertFilterNoWriteBack(localThreadData, key, 1);
            #elif USE_FILTER
            insertFilterWithWriteBack(localThreadData, key);
            #elif SINGLE
            delegateInsert(localThreadData, key, 1, 0);
            #else
            insert(localThreadData, key, 1);
            #endif
        }
        //If duration is 0 then I only loop once over the input. This is to do accuracy tests.
        if (DURATION == 0){
            break;
        }
    }
    #if (!DURATION && DELEGATION_FILTERS) 
    // keep clearing your backlog, other wise we might endup in a deadlock
    serveDelegatedInsertsAndQueries(localThreadData); 
    __sync_fetch_and_add( &threadsFinished, 1);
    while( threadsFinished < numberOfThreads){
        serveDelegatedInsertsAndQueries(localThreadData); 
    }
    #endif
    localThreadData->numTopKQueries = numTopKQueries;
    localThreadData->numQueries = numQueries;
    localThreadData->numInserts = numInserts;
}


void * threadEntryPoint(void * threadArgs){
    int tid = *((int *) threadArgs);
    threadDataStruct * localThreadData = &(threadData[tid]);
    #if 0 == 1
    //ITHACA: fill first NUMA node first (even numbers)
    int thread_id = (tid%36)*2 + tid/36;
    setaffinity_oncpu(thread_id);
    #elif 2 == 2
    setaffinity_oncpu(tid);
    #else
    setaffinity_oncpu(14*(tid%2)+(tid/2)%14);
    #endif

    int threadWorkSize = tuples_no /  (numberOfThreads);
    localThreadData->startIndex = tid * threadWorkSize;
    localThreadData->endIndex =  localThreadData->startIndex + threadWorkSize; //Stop before you reach that index
    //The last thread gets the rest if tuples_no is not devisible by numberOfThreads
    //(it only matters for accuracy tests)
    if (tid == (numberOfThreads - 1)){
        localThreadData->endIndex = tuples_no;
    }
    localThreadData->pendingQueriesKeys =  (int *)calloc(numberOfThreads, sizeof(int));
    localThreadData->pendingQueriesCounts =  (unsigned int *)calloc(numberOfThreads, sizeof(unsigned int));
    localThreadData->pendingQueriesFlags =  (volatile int *)calloc(numberOfThreads, sizeof(volatile int));
    localThreadData->pendingTopKQueriesFlags = (volatile float *)calloc(numberOfThreads, sizeof(volatile int));
    for(int i=0; i<numberOfThreads; i++){
        localThreadData->pendingQueriesKeys[i] = -1;
        localThreadData->pendingTopKQueriesFlags[i] = 0.0;
    }

    struct libdivide::libdivide_s32_t fast_d = libdivide::libdivide_s32_gen((int32_t)numberOfThreads);
    localThreadData->fastDivHandle = &fast_d;

    localThreadData->insertsPending = 0;
    localThreadData->queriesPending = 0;
    localThreadData->listOfFullFilters = NULL;
    localThreadData->seeds = seed_rand();

    #if PREINSERT
    // insert the input once to prepare the sketches and the filters
    // FIXME: reuse the code from threadWork
    int start = localThreadData->startIndex;
    int end = localThreadData->endIndex;
    for (int i = start; i < end; i++)
    {
        unsigned int key = (*localThreadData->theData->tuples)[i];
        #if DELEGATION_FILTERS
        serveDelegatedInserts(localThreadData);
        //int old_owner = key - numberOfThreads * libdivide::libdivide_s32_do((uint32_t)key, fastDivHandle);
        int owner = findOwner(key);
        delegateInsert(localThreadData, key, 1, owner);
        #elif AUGMENTED_SKETCH
        insertFilterNoWriteBack(localThreadData, key, 1);
        #elif USE_FILTER
        insertFilterWithWriteBack(localThreadData, key);
        #else
        insert(localThreadData, key, 1);
        #endif
        serveDelegatedInsertsAndQueries(localThreadData); 
        __sync_fetch_and_add( &threadsFinished, 1);
        while( threadsFinished < numberOfThreads){
            serveDelegatedInsertsAndQueries(localThreadData); 
        }
    }
    #endif
    barrier_cross(&barrier_global);
    barrier_cross(&barrier_started);
    threadWork(localThreadData);

    return NULL;
}

void postProcessing(){

    long int sumNumQueries=0, sumNumInserts = 0, sumTopKQueries = 0;
    double sumReturnValues = 0;
    for (int i=0; i<numberOfThreads; i++){
        sumNumQueries += threadData[i].numQueries;
        sumNumInserts += threadData[i].numInserts;
        sumReturnValues += threadData[i].returnData;
        sumTopKQueries += threadData[i].numTopKQueries;
    }
    float percentage  = (float) sumNumQueries * 100/(sumNumQueries + sumNumInserts);
    float percentagetopk  = (float) sumTopKQueries *100 / (sumNumInserts+sumTopKQueries);
    printf("LOG: num Queries: %ld, num Inserts %ld, percentage %f num topk %ld, topk percentage %f, garbage print %f\n",sumNumQueries, sumNumInserts, percentage, sumTopKQueries, percentagetopk, sumReturnValues);
}

void printAccuracyResults(vector<pair<uint32_t,uint32_t>>*vecthist,vector<pair<uint32_t,uint32_t>>*lasttopk, uint64_t sumNumInserts){
        // Calculate Recall, Precision and Average Relative Error
        set<uint32_t> truth;
        set<uint32_t> elems;
        set<uint32_t> true_positives;

        for (int i = 0; i < vecthist->size(); i++){
            if (vecthist->at(i).second > ceil(sumNumInserts*PHI)){
                truth.insert(vecthist->at(i).first);
            }        
        }
        for (int i = 0; i < lasttopk->size(); i++){
            elems.insert(lasttopk->at(i).first);     
        }
        for (int i = 0; i < lasttopk->size(); i++){
            if (truth.find(lasttopk->at(i).first) != truth.end() ){
                true_positives.insert(lasttopk->at(i).first);
            } 
        }
        float avg_rel_error=0;
        for (int i = 0; i < lasttopk->size(); i++){
            for (int j = 0; j < vecthist->size(); j++){
                if (lasttopk->at(i).first == vecthist->at(j).first){
                    float rel_error=abs(1-((float)lasttopk->at(i).second/(float)vecthist->at(j).second)); 
                    avg_rel_error+=rel_error;
                    break;
                }
            }
        }
        avg_rel_error/=lasttopk->size();
        float recall;
        float precision;
        if (elems.size()==0){ // If the algorithm returns nothing, then precision/recall is 1 and per-element error is 0.
            recall=1;
            precision=1;
            avg_rel_error=0;
        }
        else{
            recall=(float)true_positives.size()/(float)truth.size();
            precision=(float)true_positives.size()/(float)elems.size();
        }
        printf("\nElements: %d, Truth:%d, True Positives:%d",elems.size(),truth.size(),true_positives.size());
        printf("\nPrecision:%f, Recall:%f, AverageRelativeError:%f\n", precision,recall,avg_rel_error );
        printf("\n");
}
void saveAccuracyHistogram(vector<pair<uint32_t,uint32_t>>*vecthist,vector<pair<uint32_t,uint32_t>>*lasttopk,uint64_t sum){
        FILE *fp = fopen("logs/topk_results.txt", "w");
        //N, K,Phi in first row.
        fprintf(fp,"%llu %d %f\n",sum,K,PHI);
        for (int i = 0; i < vecthist->size(); i++){
            pair<uint32_t,uint32_t> ltopk;
            if (i < lasttopk->size()){
                ltopk = lasttopk->at(i);
            }
            else{
                ltopk = pair<uint32_t,uint32_t>(-1,0);
            }
            fprintf(fp, "%d %u %u %u %u %d\n",i, vecthist->at(i).first,vecthist->at(i).second, ltopk.first,ltopk.second, (vecthist->at(i).first == ltopk.first));
            if (i == lasttopk->size()+100)
                break;
        }
        fclose(fp);
}

vector<unsigned int> *read_ints(const char *file_name, uint64_t *length, unordered_map<uint32_t,uint32_t>* hist_um)
{
    printf("started reading input\n");
    FILE *file = fopen(file_name, "r");
    unsigned int i = 0;

    //read number of values in the file
    //fscanf(file, "%d", &i);
    //unsigned int *input_values = (unsigned int *)calloc(i, sizeof(unsigned int));
    vector <unsigned int> * input_values = new vector<unsigned int>();

    //fscanf(file, "%d", &i);
    int index = 0;
    while (!feof(file))
    {
        fscanf(file, "%d", &i);
        input_values->push_back(i);
        index++;
        auto hist_um_it = hist_um->find(i);
        if (hist_um_it != hist_um->end()) {
            hist_um_it->second++;
        }
        else {
            hist_um->insert(std::make_pair(i , (uint32_t)1));
        }
    }
    *length=index;
    fclose(file);
    printf("%llu elements read, %u uniques\n",*length,hist_um->size());
    return input_values;
}


uint64_t getGroundTruth(vector<pair<uint32_t,uint32_t>>* vecthist,uint32_t* hist1,int use_real_data,uint32_t dom_size,unordered_map<uint32_t,uint32_t>* hist_um){
    uint64_t sum=0;
    if (use_real_data){
        for (auto& it: *hist_um) {
            vecthist->push_back(std::pair<uint32_t,uint32_t>(it.first,it.second));
            sum+=it.second;
        }
    }
    else{
        for(int i =0;i< dom_size;i++){
            vecthist->push_back(std::pair<uint32_t,uint32_t>(i,(hist1)[i]));
            sum+=(hist1)[i];
        }
    }
    sort(vecthist->begin(), vecthist->end(), [vecthist](pair<uint32_t,uint32_t> p1, pair<uint32_t,uint32_t> p2) {return p1.second > p2.second;});
    return sum;
}


void saveMemoryConsumption(LCL_type* ss, int numberOfThreads){
    FILE *fp;
    fp = fopen("logs/topk_space.txt", "w");
    int memorysize=LCL_Size(ss);
    int numcounters=ss->size;
    #if !SINGLE
    numcounters*=(numberOfThreads);
    memorysize*=(numberOfThreads);
    // Incl deleg filters and counters
    memorysize+= (sizeof(FilterStruct) * (numberOfThreads)*(numberOfThreads)) +  (sizeof(uint32_t) * MAX_FILTER_UNIQUES * 2 *(numberOfThreads)*(numberOfThreads)) + sizeof(uint64_t)*numberOfThreads + sizeof(atomic_flag)*(numberOfThreads)  ; 
    // T^2 filters + size of keys and values arrays +  T * counters + flags
    #endif
    fprintf(fp,"%d %d\n",memorysize,numcounters);
    fclose(fp);
}

int main(int argc, char **argv)
{
    uint32_t dom_size;
    int buckets_no;

    int DIST_TYPE;
    double DIST_PARAM, DIST_SHUFF;

    int runs_no;

   if ((argc != 18) && (argc != 19))
    {
        printf("Usage: sketch_compare.out dom_size tuples_no buckets_no rows_no DIST_TYPE DIST_PARAM DIST_DECOR runs_no num_threads querry_rate duration(in sec, 0 means one pass over the data), (optional) input_file_name \n");
        exit(1);
    }
    int use_real_data = 0;
    char input_file_name[1024];
    vector<unsigned int> * input_data;
    uint64_t input_length;
    //Histogram for real-world data:
    unordered_map<uint32_t,uint32_t> hist_um;

    if (argc==19){
        use_real_data = 1;
        strcpy(input_file_name,argv[18]);
        input_data = read_ints(input_file_name, &input_length,&hist_um);
    }

    dom_size = atoi(argv[1]);
    tuples_no = atoi(argv[2]);
    if (use_real_data){
        tuples_no = input_length;
        dom_size=hist_um.size();
    }

    buckets_no = atoi(argv[3]);
    rows_no = atoi(argv[4]);

    DIST_TYPE = atoi(argv[5]);
    DIST_PARAM = atof(argv[6]);
    DIST_SHUFF = atof(argv[7]);

    runs_no = atoi(argv[8]);

    numberOfThreads = atoi(argv[9]);

    QUERRY_RATE = atoi(argv[10]);

    DURATION = atoi(argv[11]);

    COUNTING_PARAM=atoi(argv[12]);

    TOPK_QUERY_RATE=atoi(argv[13]);

    K=atoi(argv[14]);

    PHI=atof(argv[15]);

    MAX_FILTER_SUM = atoi(argv[16]);

    MAX_FILTER_UNIQUES = atoi(argv[17]);

    //srand((unsigned int)time((time_t *)NULL));
    srand(0);

    //Ground truth histrogram
    unsigned int *hist1 = (unsigned int *)calloc(dom_size, sizeof(unsigned int));

    //generate the two relations
    Relation *r1 = new Relation(dom_size, tuples_no);

    if (use_real_data){
        r1->tuples = input_data;
    }
    else{
        r1->Generate_Data(DIST_TYPE, DIST_PARAM, DIST_SHUFF); //Note last arg should be 1
    }
    //if (r1->tuples_no < tuples_no){ //Sometimes Generate_Data might generate less than tuples_no
    tuples_no = r1->tuples_no;
    //}
    auto rng = default_random_engine {};
    shuffle(begin((*r1->tuples)), end((*r1->tuples)), rng);


    int c = 0;
    for (int i=0; i<512; i++){
        precomputedMods[i] = c;
        c++;
        c = c % (numberOfThreads);
    }  

    for (int jj = 0; jj < runs_no; jj++)
    {
        unsigned int I1, I2;

        //generate the pseudo-random numbers for CM sketches; use CW2B
        //NOTE: doesn't work with CW2B, need to use CW4B. Why?
        Xi **cm_cw2b = new Xi *[rows_no];
        for (int i = 0; i < rows_no; i++)
        {
            I1 = Random_Generate();
            I2 = Random_Generate();
            cm_cw2b[i] = new Xi_CW4B(I1, I2, buckets_no);
        }

        if (!use_real_data){
            for (int i = 0; i < dom_size; i++)
            {   
                hist1[i] = 0;
            }
        }

        printf("size of the sketch %zu\n",sizeof(Count_Min_Sketch));
        globalSketch = new Count_Min_Sketch(buckets_no, rows_no, cm_cw2b);
        Count_Min_Sketch ** cmArray = (Count_Min_Sketch **) aligned_alloc(64, (numberOfThreads) * sizeof(Count_Min_Sketch *));
        Frequent_CM_Sketch ** topkapi = (Frequent_CM_Sketch **) aligned_alloc(64, (numberOfThreads) * sizeof(Frequent_CM_Sketch *));
        LossySketch *  th_local_sketch = (LossySketch* ) malloc(rows_no*numberOfThreads*
                                            sizeof(LossySketch));


        for (int i=0; i<numberOfThreads; i++){
            cmArray[i] = new Count_Min_Sketch(buckets_no, rows_no, cm_cw2b);
            topkapi[i] = new Frequent_CM_Sketch(buckets_no,rows_no,cm_cw2b);
            cmArray[i]->SetGlobalSketch(globalSketch);
            for (int th_i = 0; th_i < rows_no; ++th_i){
                allocate_sketch( &th_local_sketch[i * rows_no + th_i], buckets_no);
            }
        }

        filterMatrix = (FilterStruct *) calloc((numberOfThreads)*(numberOfThreads), sizeof(FilterStruct));
        for (int thread = 0; thread< (numberOfThreads)*(numberOfThreads); thread++){
            filterMatrix[thread].filter_id = (uint32_t *) calloc(MAX_FILTER_UNIQUES,sizeof(uint32_t));
            filterMatrix[thread].filter_count = (uint32_t *) calloc(MAX_FILTER_UNIQUES,sizeof(uint32_t));
            for (int j=0; j< MAX_FILTER_UNIQUES; j++){
                filterMatrix[thread].filterSum=0;
                filterMatrix[thread].filterCount=0;
                filterMatrix[thread].filterFull=0;
            }
        }

        if (!use_real_data){
            for (int i = 0; i < r1->tuples_no; i++)
            {
                hist1[(*r1->tuples)[i]]++;
                uniques.insert((*r1->tuples)[i]);
            }
            uniques_no=uniques.size();
        }
        initThreadData(cmArray,r1,MAX_FILTER_SUM,MAX_FILTER_UNIQUES,topkapi,TOPK_QUERY_RATE,tuples_no,numberOfThreads,th_local_sketch,cm_cw2b);
        spawnThreads();
        barrier_cross(&barrier_global);        
        #if PREINSERT
        threadsFinished = 0;
        #endif

        startTime();
        
        startBenchmark = 1;
        if (DURATION > 0){
            sleep(DURATION);
            startBenchmark = 0;
        }
        collectThreads();
        stopTime();
        #if ACCURACY 
        // Perform a query at the end of the stream
        #if SPACESAVING
        FEquery(&(threadData[0]),K,PHI,&(threadData[0].lasttopk));
        #elif TOPKAPI
        for (int i = 0; i < rows_no; ++i){
            local_merge_sketch(th_local_sketch, numberOfThreads, rows_no, i);
        }
        std::map<int,int> topk_words;
        std::map<int,int>::reverse_iterator rit;
        int num_heavy_hitter = 0;
        int count;
        uint32_t elem;
        int i,j;
        int id;
        int range=buckets_no;
        int frac_epsilon=K*10;
        int* is_heavy_hitter = (int* )malloc(range*sizeof(int));
        int threshold = (int) ((range/K) - 
                            (range/frac_epsilon));

        for (i = 0; i < range; ++i){
            is_heavy_hitter[i] = FALSE;
            for (j = 0; j < rows_no; ++j){
                if ( j == 0){
                elem = th_local_sketch->identity[i];
                count = th_local_sketch->lossyCount[i];
                if (count >= threshold)
                {
                    is_heavy_hitter[i] = TRUE;
                }
                } else {
                id = threadData->randoms[j]->element(elem);
                if ((th_local_sketch[j].identity[id] !=  elem) )
                {
                    continue;
                } else if (th_local_sketch[j].lossyCount[id] >= threshold)
                {
                    is_heavy_hitter[i] = TRUE;
                }
                if (th_local_sketch[j].lossyCount[id] > count)
                    count = th_local_sketch[j].lossyCount[id];
                }
            }
            th_local_sketch->lossyCount[i] = count;
            }

            for (i = 0; i < range; ++i)
            {
            if (is_heavy_hitter[i])
            {
                num_heavy_hitter ++;
                topk_words.insert( std::pair<int,int>(th_local_sketch->lossyCount[i], i) );
            }
            }

            for (i = 0, rit = topk_words.rbegin(); 
                (i < K) && (rit != topk_words.rend()); 
                    ++i, ++rit)
            {
            j = rit->second;
            printf( "%u %d\n", th_local_sketch->identity[j], 
                        rit->first);
        }
        /* free memories */
        free(is_heavy_hitter); 
        
        //topkapi_query(&(threadData[0]),K,PHI,&(threadData[0].lasttopk));
        #endif
        #endif
        postProcessing();
        
        long int sumNumQueries=0, sumNumInserts = 0, sumTopKQueries = 0;
        double sumReturnValues = 0;
        for (int i=0; i<numberOfThreads; i++){
        sumNumQueries += threadData[i].numQueries;
        sumNumInserts += threadData[i].numInserts;
        sumReturnValues += threadData[i].returnData;
        sumTopKQueries += threadData[i].numTopKQueries;
    }

    printf("Total insertion time (ms): %lu\n",getTimeMs());
    uint64_t totalFiltersInserted=0;
    uint64_t totalFiltersums=0;
    for (int i=0; i<numberOfThreads; i++){
        totalFiltersInserted+=threadData[i].numInsertedFilters;
        totalFiltersums+=threadData[i].accumFilters;
        #if (! SINGLE) && DEBUG
        printf("id:%d number of filters:%u, num items: %llu avg:%f\n",threadData[i].tid,threadData[i].numInsertedFilters,threadData[i].accumFilters,threadData[i].accumFilters/(double)threadData[i].numInsertedFilters);
        #endif
        //printf("thread: %d num uniques: %d\n",threadData[i].tid,threadData[i].num_uniques);
    }

    #if LATENCY
    const float clockspeed_hz = 3066775000.0; // clockspeed of the server
    float average=0.0;
    for(int i=0;i<numberOfThreads;i++){
        for(int j=0;j<threadData[i].numTopKQueries;j++){
            average+= threadData[i].latencies[j] / clockspeed_hz;
        }
        average/=threadData[i].numTopKQueries;
        average*= pow(10,6); // convert to microseconds
        printf("\nthread: %d avg: %f\n",i,average);
        average=0.0;
    }
    #endif


    #if ACCURACY //Accuracy results
    vector<pair<uint32_t,uint32_t>> vecthist; // Ground truth
    vector<pair<uint32_t,uint32_t>> lasttopk=threadData[0].lasttopk; // Query at the end of the stream
    uint64_t sumGroundTruth=getGroundTruth(&vecthist,hist1,use_real_data,dom_size,&hist_um);
    printAccuracyResults(&vecthist,&lasttopk,sumGroundTruth);
    saveAccuracyHistogram(&vecthist,&lasttopk,sumGroundTruth);
    saveMemoryConsumption(threadData[0].ss,numberOfThreads);
    #endif

    #if (! SINGLE) && DEBUG
    printf("Number of items per full filter on average: %u \n ", totalFiltersums/totalFiltersInserted);
    #endif
    printf("Insertion throughput %f MOps per sec\n", (float)sumNumInserts / getTimeMs() / 1000);
    printf("Query throughput %f MQueries per sec\n", (float)sumTopKQueries / getTimeMs() / 1000);
    printf("Total processing throughput %f MInserts per sec\n", (float)sumNumInserts / getTimeMs() / 1000);
    
    for (int i = 0; i < rows_no; i++)
    {
        delete cm_cw2b[i];
    }

    delete[] cm_cw2b;

    for (int i=0; i<numberOfThreads; i++){
        delete cmArray[i];
        LCL_Destroy(threadData[i].ss);
    }
    free(cmArray);
    
    printf("-----------------------\n");
    printf("DURATION:            %d\n", DURATION);
    printf("QUERRY RATE:         %d\n", QUERRY_RATE);
    printf("THREADS:             %d\n", numberOfThreads);
    printf("True uniques:             %u\n", uniques_no);

    }
    hist1 = NULL;
    delete r1;

    return 0;
}
