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
#include <cstring>
#include <string>
#include <unordered_map>
#include <set>
#include <fstream>
#include <iterator>
#include <sstream>
#include <iostream>

#define NO_SQUASHING 0
#define HASHA 151261303
#define HASHB 6722461
#define TRUE 1
#define FALSE 0

using namespace std;

FilterStruct * filterMatrix;

int K;
float PHI;
int MAX_FILTER_SUM,MAX_FILTER_UNIQUES;
int tuples_no,rows_no,buckets_no;


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
    (*sketch).identity[i] = -1;
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
    return precomputedMods[key & 511];
}
volatile int threadsFinished = 0;

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

        // parse filter and add each element to your own filter
        for (int i=0; i<filter->filterCount;i++){
            uint32_t count = filter->filter_count[i];
            uint32_t key = filter->filter_id[i];
            #if SPACESAVING
            LCL_Update(localThreadData->ss,key,count);
            #else // If vanilla Delegation Sketch is used
            insertFilterNoWriteBack(localThreadData, key, count);
            #endif
            // flush each element
            filter->filter_id[i] = -1;
            filter->filter_count[i] = 0;
        }
        // mark filter as empty
        filter->filterCount = 0;
        filter->filterFull = false;
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
    serveDelegatedQueries(localThreadData); 
}

static inline void insertSingleSS(threadDataStruct * localThreadData, unsigned int key){
    LCL_Update(localThreadData->ss,key,1);
    localThreadData->substreamSize++;
	return;
}

static inline void delegateInsert(threadDataStruct * localThreadData, unsigned int key, unsigned int increment, int owner){
    FilterStruct* filter = &(filterMatrix[localThreadData->tid * (numberOfThreads) + owner]);

    InsertInDelegatingFilterWithListAndMaxSum(filter, key);
    localThreadData->sumcounter++;
    localThreadData->substreamSize++;

    
	// If the current filter contains max uniques, or if the number of inserts since last window is equal to max, flush all filters 
    if (filter->filterCount == MAX_FILTER_UNIQUES || localThreadData->sumcounter == MAX_FILTER_SUM ){
        // push all filters to the other threads 
        for (int i=0;i< numberOfThreads;i++){
            filter = &(filterMatrix[localThreadData->tid * (numberOfThreads) + i]);
            if ( filter->filterCount > 0){
                threadDataStruct * owningThread = &(threadData[i]);
                filter->filterFull = true;
                push(filter, &(owningThread->listOfFullFilters));
            }
        }

        // Make sure all filters are empty before continuing
        for (int i=0;i< numberOfThreads;i++){
            filter = &(filterMatrix[localThreadData->tid * (numberOfThreads) + i]);
            while(filter->filterFull && startBenchmark){
                serveDelegatedInserts(localThreadData);
            }
        }
        localThreadData->sumcounter=0;     
    }
    
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

void topkapi_query_merge(threadDataStruct *localThreadData, int range,int num_topk){
        num_topk=std::max(1,num_topk);
        localThreadData->lasttopk.clear();
        LossySketch* th_local_sketch = localThreadData->th_local_sketch;
        LossySketch*  merged = (LossySketch* ) malloc(rows_no*sizeof(LossySketch));
        for (int th_i = 0; th_i < rows_no; ++th_i){
            allocate_sketch( &merged[th_i], range);
        }
        for (int i = 0; i < rows_no; ++i){
            local_merge_sketch(merged, th_local_sketch, numberOfThreads, rows_no, i);
        }
        std::map<int,int> topk_words;
        std::map<int,int>::reverse_iterator rit;
        int num_heavy_hitter = 0;
        int count;
        uint32_t elem;
        int i,j;
        int id;
        int frac_epsilon=num_topk*10;
        int* is_heavy_hitter = (int* )malloc(range*sizeof(int));
        int threshold = (int) ((range/num_topk) - 
                            (range/frac_epsilon));

        for (i = 0; i < range; ++i){
            is_heavy_hitter[i] = FALSE;
            for (j = 0; j < rows_no; ++j){
                if ( j == 0){
                    elem = merged->identity[i];
                    count = merged->lossyCount[i];
                    if (count >= threshold)
                    {
                        is_heavy_hitter[i] = TRUE;
                    }
                } 
                else {
                    id = threadData->randoms[j]->element(elem);
                if ((merged[j].identity[id] !=  elem) )
                {
                    continue;
                } else if (merged[j].lossyCount[id] >= threshold)
                {
                    is_heavy_hitter[i] = TRUE;
                }
                if (merged[j].lossyCount[id] > count)
                    count = merged[j].lossyCount[id];
                }
            }
            merged->lossyCount[i] = count;
            }

            for (i = 0; i < range; ++i)
            {
                if (is_heavy_hitter[i])
                {
                    num_heavy_hitter ++;
                    topk_words.insert( std::pair<int,int>(merged->lossyCount[i], i) );
                }
            }

            for (i = 0, rit = topk_words.rbegin(); 
                (i < num_topk) && (rit != topk_words.rend()); 
                    ++i, ++rit)
            {
                j = rit->second;
                localThreadData->lasttopk.push_back(make_pair(merged->identity[j],rit->first));
            }
        /* free memories */
        for (int th_i = 0; th_i < rows_no; ++th_i){
            deallocate_sketch( &merged[th_i]);
        }
        free(merged);
        free(is_heavy_hitter);
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

// Performs a frequent elements query 
void FEquery(threadDataStruct * localThreadData,float phi,vector<pair<uint32_t,uint32_t>>* v ){
    //float cardinality_estimate=0.0;
    LCL_type* local_spacesaving;
    v->resize(0);
    uint64_t streamsize=0;
    
    // If single threaded just extract frequent elements from the local Space-Saving instance
    #if SINGLE
    streamsize = localThreadData->substreamSize;
    local_spacesaving = localThreadData->ss;
    LCL_Output(local_spacesaving,streamsize*phi,v);
    //cardinality_estimate=threadData[0].sum_of_buckets;

    // If Delegation Space-Saving then extract local frequent elements at all threads
    #else

    // Estimate stream size across all threads
    for (int j=0;j<numberOfThreads;j++){
        streamsize += threadData[j].substreamSize;
    }

    // Get all local frequent elements at the threads
    bool bm[numberOfThreads]={0};
    int num_complete=0;
    int i=0;
    while (num_complete < numberOfThreads){
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
            num_complete++;
            serveDelegatedInserts(localThreadData);
        }
        i=precomputedMods[i+1];
    }
    #endif

    // Sort output
    
}
void threadWork(threadDataStruct *localThreadData)
{
    int start = localThreadData->startIndex;
    int end = localThreadData->endIndex;
    int i;
    int numOps = 0;
    int numQueries = 0;
    int numTopKQueries = 0;
    while (!startBenchmark)
    {
    }
    while (startBenchmark)
    {
        for (i = start; i < end; i++)
        {
            if (!startBenchmark ){
                break;
            }
            unsigned int key = (*localThreadData->theData->tuples)[i];
            if (shouldTopKQuery(localThreadData) < TOPK_QUERY_RATE)
            {
                #if LATENCY
                uint64_t tick = rdtsc();  // Latency measurement
                #endif

                #if SPACESAVING
                FEquery(localThreadData,PHI,&(localThreadData->lasttopk));
                #elif TOPKAPI
                //topkapi_query(localThreadData,K,PHI,&(localThreadData->lasttopk));
                topkapi_query_merge(localThreadData,buckets_no,K);
                #endif

                #if LATENCY
                localThreadData->latencies[numTopKQueries >= 2000000 ? numTopKQueries % 2000000 : numTopKQueries]=rdtsc() - tick;
                //printf("after: %lu\n",rdtsc() - tick); // Latency measurement
                #endif
                numTopKQueries++;
            }                        
            numOps++;
            #if TOPKAPI
            insert(localThreadData,key,1);
            #elif DELEGATION_FILTERS
            serveDelegatedInserts(localThreadData);
            // int old_owner = key - numberOfThreads * libdivide::libdivide_s32_do((uint32_t)key, fastDivHandle);
            int owner = findOwner(key);
            delegateInsert(localThreadData, key, 1, owner);
            #elif SINGLE
			insertSingleSS(localThreadData,key);
            #endif
        }
        //If duration is 0 then I only loop once over the input. This is to do accuracy tests.
        if (DURATION == 0){
            break;
        }
    }
    #if (!DURATION && DELEGATION_FILTERS) 
    // keep clearing your backlog, other wise we might endup in a deadlock
    serveDelegatedInserts(localThreadData); 
    __sync_fetch_and_add( &threadsFinished, 1);
    while( threadsFinished < numberOfThreads){
        serveDelegatedInserts(localThreadData); 
    }
    #endif
    localThreadData->numTopKQueries = numTopKQueries;
    localThreadData->numQueries = numQueries;
    localThreadData->numOps = numOps;
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

    localThreadData->insertsPending = 0;
    localThreadData->queriesPending = 0;
    localThreadData->listOfFullFilters = NULL;
    localThreadData->seeds = seed_rand();    
    // Insert the data once, here we let each thread process the whole dataset,
    // and let the threads cherry-pick the elements that they own.
    #if PREINSERT
    int start,end;
    #if TOPKAPI
    start=localThreadData->startIndex;
    end=localThreadData->endIndex;
    #else
    start=0;
    end=tuples_no;
    #endif

    for (int i = start; i < end; i++){
        uint32_t key = (*localThreadData->theData->tuples)[i];
        #if DELEGATION_FILTERS
        int owner = findOwner(key);
        if (owner == localThreadData->tid){
            LCL_Update(localThreadData->ss,key,1);
            localThreadData->substreamSize++;
        }
        #elif TOPKAPI
        insert(localThreadData,key,1);
        #elif SINGLE
        LCL_Update(localThreadData->ss,key,1);
        localThreadData->substreamSize++;
        #endif
    }
    #endif
    barrier_cross(&barrier_global);
    barrier_cross(&barrier_started);
    threadWork(localThreadData);

    return NULL;
}

void postProcessing(){

    long int sumNumQueries=0, sumNumOps = 0, sumTopKQueries = 0;
    double sumReturnValues = 0;
    for (int i=0; i<numberOfThreads; i++){
        sumNumQueries += threadData[i].numQueries;
        sumNumOps += threadData[i].numOps;
        sumReturnValues += threadData[i].returnData;
        sumTopKQueries += threadData[i].numTopKQueries;
    }
    float percentage  = (float) sumNumQueries * 100/(sumNumOps);
    float percentagetopk  = (float) sumTopKQueries *100 / (sumNumOps);
    printf("LOG: num Queries: %ld, num Inserts %ld, percentage %f num topk %ld, topk percentage %f, garbage print %f\n",sumNumQueries, (sumNumOps-sumTopKQueries), percentage, sumTopKQueries, percentagetopk, sumReturnValues);
}

void printAccuracyResults(vector<pair<uint32_t,uint32_t>>*sorted_histogram,vector<pair<uint32_t,uint32_t>>*lasttopk, uint64_t sumNumOps){
        // Calculate Recall, Precision and Average Relative Error
        set<uint32_t> truth;
        set<uint32_t> elems;
        std::vector<std::pair<uint32_t,uint32_t>> true_positives;

        for (int i = 0; i < sorted_histogram->size(); i++){
            if (sorted_histogram->at(i).second > ceil(sumNumOps*PHI)){
                truth.insert(sorted_histogram->at(i).first);
            }        
        }
        for (int i = 0; i < lasttopk->size(); i++){
            elems.insert(lasttopk->at(i).first);     
        }
        for (int i = 0; i < lasttopk->size(); i++){
            if (truth.find(lasttopk->at(i).first) != truth.end() ){
                true_positives.push_back(lasttopk->at(i));
            } 
        }
        float recall;
        float precision;
        float avg_rel_error=0;
        int num_matches=0;
        for (int i = 0; i < true_positives.size(); i++){
            for (int j = 0; j < sorted_histogram->size(); j++){
                if (true_positives.at(i).first == sorted_histogram->at(j).first){
                    int abserr = abs(static_cast<int>(true_positives.at(i).second - sorted_histogram->at(j).second));
                    float rel_error = abserr / (float) sorted_histogram->at(j).second;
                    if (sorted_histogram->at(j).second == 0 || true_positives.at(i).second < 0){
                        goto after_loop;
                    }
                    avg_rel_error+=rel_error;
                    num_matches++;
                }
            }
        }
        after_loop:
        avg_rel_error/=num_matches;
        recall=(float)true_positives.size()/(float)truth.size();
        precision=(float)true_positives.size()/(float)elems.size();
        if (lasttopk->size()==0){
            avg_rel_error=0;
        }
        
        if (truth.size() == 0){
            recall=1;
        }
        if (elems.size() == 0){
            precision=1;
        }
        printf("\nElements: %d, Truth:%d, True Positives:%d",elems.size(),truth.size(),true_positives.size());
        printf("\nPrecision:%f, Recall:%f, AverageRelativeError:%f\n", precision,recall,avg_rel_error );
        printf("\n");
}
void saveAccuracyHistogram(vector<pair<uint32_t,uint32_t>>*sorted_histogram,vector<pair<uint32_t,uint32_t>>*lasttopk,uint64_t sum){
        FILE *fp = fopen("logs/topk_results.txt", "w");
        //N, K,Phi in first row.
        fprintf(fp,"%llu %d %f\n",sum,K,PHI);
        for (int i = 0; i < sorted_histogram->size(); i++){
            pair<uint32_t,uint32_t> ltopk;
            if (i < lasttopk->size()){
                ltopk = lasttopk->at(i);
            }
            else{
                ltopk = pair<uint32_t,uint32_t>(-1,0);
            }
            fprintf(fp, "%d %u %u %u %u %d\n",i, sorted_histogram->at(i).first,sorted_histogram->at(i).second, ltopk.first,ltopk.second, (sorted_histogram->at(i).first == ltopk.first));
            if (i == lasttopk->size()+100)
                break;
        }
        fclose(fp);
}

void read_ints(const char *file_name, vector<uint32_t>* input_data , vector<uint32_t>* histogram)
{
    printf("started reading input\n");
    FILE *file = fopen(file_name, "r");
    unsigned int i = 0;
    
    while (!feof(file))
    {
        fscanf(file, "%d", &i);
        input_data->push_back(i);
        histogram->at(i)+=1;
    }
    fclose(file);
    printf("Done reading input\n");
}


uint64_t getSortedHistogram(vector<pair<uint32_t,uint32_t>>& sorted_histogram,vector<uint32_t>* histogram){
    uint64_t sum=0;
    
    // Convert histogram to pairs
    uint32_t element=0;
    for (uint32_t value: *histogram) {
        if (value > 0){
            sorted_histogram.push_back(std::pair<uint32_t,uint32_t>(element,value));
            sum+=value;
        }
        element++;
    } 
    delete histogram;

    //Sort pairs
    sort(sorted_histogram.begin(), sorted_histogram.end(), [](pair<uint32_t,uint32_t> p1, pair<uint32_t,uint32_t> p2) {return p1.second > p2.second;});
    return sum;
}


void saveMemoryConsumption(threadDataStruct* localThreadData, int numberOfThreads){
    int memorysize, numcounters;
    FILE *fp;
    fp = fopen("logs/topk_space.txt", "w");
    #if TOPKAPI 
    memorysize = numberOfThreads*topk_size(localThreadData->th_local_sketch,rows_no);
    numcounters = numberOfThreads * localThreadData->th_local_sketch->_b * rows_no;
    #else 
    LCL_type* ss = localThreadData->ss;
    memorysize=LCL_Size(ss);
    numcounters=ss->size;
    #if !SINGLE
    numcounters*=(numberOfThreads);
    memorysize*=(numberOfThreads);
    // Incl deleg filters and counters
    memorysize+= (sizeof(FilterStruct) * (numberOfThreads)*(numberOfThreads)) +  (sizeof(uint32_t) * MAX_FILTER_UNIQUES * 2 *(numberOfThreads)*(numberOfThreads)) + sizeof(uint64_t)*numberOfThreads + sizeof(atomic_flag)*(numberOfThreads)  ; 
    // T^2 filters + size of keys and values arrays +  T * counters + flags
    #endif
    #endif
    fprintf(fp,"%d %d\n",memorysize,numcounters);
    fclose(fp);
}

std::string to_string_trim_zeros(double a){
    // Print value to a string
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << a;
    std::string str = ss.str();
    // Ensure that there is a decimal point somewhere (there should be)
    if(str.find('.') != std::string::npos)
    {
        // Remove trailing zeroes
        str = str.substr(0, str.find_last_not_of('0')+1);
        // If the decimal point is now the last character, remove that as well
        if(str.find('.') == str.size()-1)
        {
            str = str.substr(0, str.size()-1);
        }
    }
    return str;
}

void generateDatasets(){
    for (int size : {1000000,10000000,100000000}){ 
        for (double parameter : {0.5,0.75,1.0,1.25,1.5,1.75,2.0,2.25,2.5,2.75,3.0}){
            // Generate data and shuffle
            Relation *r1 = new Relation(size, size);
            r1->Generate_Data(1, parameter, 0);
            auto rng = default_random_engine {};
            shuffle(begin((*r1->tuples)), end((*r1->tuples)), rng);

            // write data to file
            std::ofstream output_file("datasets/zipf_" + to_string_trim_zeros(parameter) + "_"  + std::to_string(size) +  ".txt");
            std::ostream_iterator<std::uint32_t> output_iterator(output_file, " ");
            std::copy(r1->tuples->begin(), r1->tuples->end(), output_iterator);
            output_file.close();
        }
    }
}

int main(int argc, char **argv)
{
    #if GENERATE_MODE
        generateDatasets();
        exit(0);
    #endif

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
    //Histogram of data distribution:
    vector<uint32_t> * histogram = new vector<uint32_t>(100000000,0);
    vector<uint32_t> * input_data = new vector<uint32_t>();
    input_data->reserve(100000000);

    if (argc==19){
        strcpy(input_file_name,argv[18]);
        read_ints(input_file_name, input_data,histogram);
    }
    else{
        printf("no input data path given, exiting\n");
        exit(1);
    }

    tuples_no = input_data->size();

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

    K=std::max(1,atoi(argv[14]));

    PHI=atof(argv[15]);

    MAX_FILTER_SUM = atoi(argv[16]);

    MAX_FILTER_UNIQUES = atoi(argv[17]);

    //srand((unsigned int)time((time_t *)NULL));
    srand(0);

    //generate the two relations
    Relation *r1 = new Relation(tuples_no, tuples_no);
    r1->tuples = input_data;
    tuples_no = r1->tuples_no;

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

        printf("size of the sketch %zu\n",sizeof(Count_Min_Sketch));
        globalSketch = new Count_Min_Sketch(buckets_no, rows_no, cm_cw2b);
        Count_Min_Sketch ** cmArray = (Count_Min_Sketch **) aligned_alloc(64, (numberOfThreads) * sizeof(Count_Min_Sketch *));
        LossySketch *  th_local_sketch = (LossySketch* ) malloc(rows_no*numberOfThreads*
                                            sizeof(LossySketch));


        for (int i=0; i<numberOfThreads; i++){
            cmArray[i] = new Count_Min_Sketch(buckets_no, rows_no, cm_cw2b);
            cmArray[i]->SetGlobalSketch(globalSketch);
            for (int th_i = 0; th_i < rows_no; ++th_i){
                allocate_sketch( &th_local_sketch[i * rows_no + th_i], buckets_no);
            }
        }

        filterMatrix = (FilterStruct *) calloc((numberOfThreads)*(numberOfThreads), sizeof(FilterStruct));
        for (int thread = 0; thread< (numberOfThreads)*(numberOfThreads); thread++){
            filterMatrix[thread].filterCount=0;
            //filterMatrix[thread].filterFull=false;
            filterMatrix[thread].filter_id = (uint32_t *) calloc(MAX_FILTER_UNIQUES,sizeof(uint32_t));
            filterMatrix[thread].filter_count = (uint32_t *) calloc(MAX_FILTER_UNIQUES,sizeof(uint32_t));
            for (int j=0; j< MAX_FILTER_UNIQUES; j++){
                filterMatrix[thread].filter_id[j]=-1;
            }
        }
        initThreadData(cmArray,r1,MAX_FILTER_SUM,MAX_FILTER_UNIQUES,TOPK_QUERY_RATE,tuples_no,numberOfThreads,th_local_sketch,cm_cw2b);
        spawnThreads();        
        barrier_cross(&barrier_global);       
        startTime();

        startBenchmark = 1;
        if (DURATION > 0){
			sleep(DURATION);
            startBenchmark = 0;
        }
        collectThreads();

        stopTime();
        
        int num_topk=0;
        vector<pair<uint32_t,uint32_t>> sorted_histogram;
        uint64_t streamsize=getSortedHistogram(sorted_histogram,histogram);
        // Find out how many heavy hitters there are
        for (int j=0;j<sorted_histogram.size();j++){
            if (sorted_histogram[j].second < streamsize*PHI){
                break;
            }
            num_topk++;
        }
        #if ACCURACY 
        // Perform a query at the end of the stream
        #if SPACESAVING
        FEquery(&(threadData[0]),PHI,&(threadData[0].lasttopk));
        #elif TOPKAPI
        topkapi_query_merge(&threadData[0],buckets_no,num_topk);
        //topkapi_query(&(threadData[0]),K,PHI,&(threadData[0].lasttopk));
        #endif
        #endif
        postProcessing();
        
        long int sumNumQueries=0, sumNumOps = 0, sumTopKQueries = 0;
        double sumReturnValues = 0;
        for (int i=0; i<numberOfThreads; i++){
            sumNumQueries += threadData[i].numQueries;
            sumNumOps += threadData[i].numOps;
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
        const double clockspeed_hz = 3066775000.0; // clockspeed of the server
        double tot_avg=0.0;
        double average=0.0;
        for(int i=0;i<numberOfThreads;i++){
            int numtopkqueries=min(2000000,threadData[i].numTopKQueries);
            for(int j=0;j<numtopkqueries;j++){
                average+= (threadData[i].latencies[j] / clockspeed_hz);
            }
            average*= pow(10,6); // nano convert to microseconds
            average/=numtopkqueries;
            tot_avg+=average;
            average=0.0;
        }
        tot_avg = tot_avg/(double)numberOfThreads;
        printf("Average latency: %lf us\n",tot_avg);
        #endif


        #if ACCURACY
        vector<pair<uint32_t,uint32_t>> lasttopk=threadData[0].lasttopk; // Query at the end of the stream
        printAccuracyResults(&sorted_histogram,&lasttopk,streamsize);
        saveAccuracyHistogram(&sorted_histogram,&lasttopk,streamsize);
        //saveMemoryConsumption(&threadData[0],numberOfThreads);
        #endif

        #if (! SINGLE) && DEBUG
        printf("Number of items per full filter on average: %u \n ", totalFiltersums/totalFiltersInserted);
        #endif
        printf("Insertion throughput %f MInserts per sec\n", (float) (sumNumOps- sumTopKQueries) / getTimeMs() / 1000);
        printf("Query throughput %f MQueries per sec\n", (float)sumTopKQueries / getTimeMs() / 1000);
        printf("Total processing throughput %f MOps per sec\n", (float)sumNumOps / getTimeMs() / 1000);
        
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
        printf("SKEW:                %g\n", DIST_PARAM);
        printf("NUM TOPK:            %d\n", num_topk);
        printf("PHI:                 %g\n", PHI);
        printf("FILEPATH:            %s\n", input_file_name);


    }
    delete r1;

    return 0;
}
