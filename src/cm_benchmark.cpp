#include "relation.h"
#include "xis.h"
#include "sketches.h"
#include "utils.h"
#include <utility>
#include "thread_utils.h"
#include "filter.h"
#include "getticks.h"
#include "lossycount.h"

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
using namespace std;

FilterStruct * filterMatrix;

int K;
float PHI;
int MAX_FILTER_SUM,MAX_FILTER_UNIQUES;
int tuples_no;

unsigned short precomputedMods[512];

static inline int findOwner(unsigned int key){
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
    return;
    #else
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
    //localThreadData->sumcounter++;
    /*while (filter->filterFull){
        serveDelegatedInsertsAndQueries(localThreadData);
    }*/
    tryInsertInDelegatingFilterWithListAndMaxSum(filter, key);
    //if (localThreadData->sumcounter == MAX_FILTER_SUM || filter->filterCount == MAX_FILTER_UNIQUES ){
    if (filter->filterSum == MAX_FILTER_SUM || filter->filterCount == MAX_FILTER_UNIQUES ){
        for (int i=0;i< numberOfThreads;i++){
            filter = &(filterMatrix[localThreadData->tid * (numberOfThreads) + i]);
            filter->filterFull=1;
            threadDataStruct * owningThread = &(threadData[i]);
            push(filter, &(owningThread->listOfFullFilters));
        }
        
        for (int i=0;i< numberOfThreads;i++){
            filter = &(filterMatrix[localThreadData->tid * (numberOfThreads) + i]);
            while( filter->filterFull  && startBenchmark){  
                serveDelegatedInsertsAndQueries(localThreadData);
            }
        }  
        //localThreadData->sumcounter=0;     
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
#if USE_MPSC
    int owner = findOwner(key); 
    localThreadData->sketchArray[owner]->enqueueRequest(key);
    localThreadData->theSketch->serveAllRequests(); //Serve any requests you can find in your own queue
#elif REMOTE_INSERTS
    int owner = findOwner(key);
    localThreadData->sketchArray[owner]->Update_Sketch_Atomics(key, increment);
#elif HYBRID
    localThreadData->theSketch->Update_Sketch_Hybrid(key, 1.0, HYBRID);
#elif LOCAL_COPIES || AUGMENTED_SKETCH || DELEGATION_FILTERS
    localThreadData->theSketch->Update_Sketch(key, double(increment));
#elif SHARED_SKETCH
    localThreadData->theGlobalSketch->Update_Sketch_Atomics(key, increment);
#endif
}

// Performs a frequent elements query 
void FEquery(threadDataStruct * localThreadData,int K,float phi,vector<pair<uint32_t,uint32_t>>* v ){
    LCL_type* local_spacesaving;
    v->clear();
    vector<uint32_t> vkeys;
    vector<uint32_t> vvals;
    uint64_t streamsize=0;
    
    // If single threaded just extract frequent elements from the local Space-Saving instance
    #if SINGLE
    streamsize = localThreadData->counter;
    local_spacesaving = localThreadData->ss;
    LCL_Output(local_spacesaving,streamsize*phi,&vkeys,&vvals);

    // If Delegation Space-Saving then extract local frequent elements at all threads
    #else

    // Estimate stream size across all threads
    for (int j=0;j<numberOfThreads;j++){
        streamsize += threadData[j].counter;
    }

    // Get all local frequent elements at the threads
    bool bm[numberOfThreads]={0};
    int num_complete=0;
    int i=0;
    while (num_complete < numberOfThreads){
        if (!bm[i]){
            if(pthread_mutex_trylock(&threadData[i].mutex)){
                // try next
                i=(i+1) % (numberOfThreads);
                continue;
            }
            local_spacesaving=threadData[i].ss;
            LCL_Output(local_spacesaving,streamsize*phi,&vkeys,&vvals);
            pthread_mutex_unlock(&threadData[i].mutex);
            bm[i]=true;
            num_complete++;
        }
        serveDelegatedInserts(localThreadData);
        i=(i+1) % (numberOfThreads);
    }
    #endif

    // Sort indices, to avoid swapping both key and value
    std::vector<int> indices(vvals.size());
    std::iota(indices.begin(),indices.end(),0);
    std::sort(indices.begin(), indices.end(),
       [&vvals](size_t i1, size_t i2) {return vvals[i1] > vvals[i2];});
    // Insert into single vector, as key:value pairs.  
    for (int ix : indices){
        v->push_back(std::pair<uint32_t,uint32_t>(vkeys[ix],vvals[ix]));
    }
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
                numTopKQueries++;
                FEquery(localThreadData,K,PHI,&(localThreadData->lasttopk));
            }                        
            numInserts++;
            #if DELEGATION_FILTERS
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

void printAccuracyResults(vector<pair<uint32_t,uint32_t>>*vecthist,vector<pair<uint32_t,uint32_t>>*lasttopk, int sumNumInserts){
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
void saveAccuracyHistogram(vector<pair<uint32_t,uint32_t>>*vecthist,vector<pair<uint32_t,uint32_t>>*lasttopk,int sum){
        FILE *fp = fopen("logs/topk_results.txt", "w");
        //N, K,Phi in first row.
        fprintf(fp,"%lld %d %f\n",sum,K,PHI);
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

vector<unsigned int> *read_ints(const char *file_name, int *length)
{
    FILE *file = fopen(file_name, "r");
    unsigned int i = 0;

    //read number of values in the file
    fscanf(file, "%d", &i);
    //unsigned int *input_values = (unsigned int *)calloc(i, sizeof(unsigned int));
    vector <unsigned int> * input_values = new vector<unsigned int>(i, 0);
    *length = i;

    fscanf(file, "%d", &i);
    int index = 0;
    while (!feof(file))
    {
        (*input_values)[index] = i;
        index++;
        fscanf(file, "%d", &i);
    }
    fclose(file);
    return input_values;
}


int getGroundTruth(vector<pair<uint32_t,uint32_t>>* vecthist,uint32_t* hist1,int use_real_data,int dom_size,unordered_map<uint32_t,uint32_t>* hist_um){
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
    int buckets_no, rows_no;

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
    int input_length;

    if (argc==19){
        use_real_data = 1;
        strcpy(input_file_name,argv[18]);
        input_data = read_ints(input_file_name, &input_length);
    }

    dom_size = atoi(argv[1]);
    tuples_no = atoi(argv[2]);
    if (use_real_data){
        tuples_no = input_length;
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
    //Histogram for real-world data:
    unordered_map<uint32_t,uint32_t> hist_um;

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

        for (int i=0; i<numberOfThreads; i++){
            cmArray[i] = new Count_Min_Sketch(buckets_no, rows_no, cm_cw2b);
            cmArray[i]->SetGlobalSketch(globalSketch);
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
            }
        }
        else{
            for (int i = 0; i < tuples_no; i++)
            {
                auto it = hist_um.find((*r1->tuples)[i]);
                if (it != hist_um.end()) {
                    it->second++;    // increment map's value for key `c`
                }       
                // key not found
                else {
                    hist_um.insert(std::make_pair((*r1->tuples)[i], 1));
                }
            }

        }
        initThreadData(cmArray,r1,MAX_FILTER_SUM,MAX_FILTER_UNIQUES);
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
        FEquery(&(threadData[0]),K,PHI,&(threadData[0].lasttopk));
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

    }
    hist1 = NULL;
    delete r1;

    return 0;
}
