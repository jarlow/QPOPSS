#include "relation.h"
#include "xis.h"
#include "sketches.h"
#include "filter.h"
#include "prng.h"

#include "utils.h"
#include "thread_utils.h"
#include "hash_utils.h"
#include "query_utils.h"
#include "getticks.h"

#include "prif.h" // PRIF implementation
#include "topkapi.h" // Topkapi implementation
#include "qpopss.h" // QPopSS implementation
#include "apache-data-sketches/frequent_items_sketch.hpp" // Apache Data Sketches implementation

#include <unistd.h>
#include <vector>
#include <unordered_map>
#include <set>

#define NO_SQUASHING 0
#define HASHA 151261303
#define HASHB 6722461
#define MAX_HISTOGRAM_SIZE 100000000

using namespace std;

FilterStruct * filterMatrix;

int K;
float PHI;
int MAX_FILTER_SUM,MAX_FILTER_UNIQUES;
int tuples_no,rows_no,buckets_no;

volatile int threadsFinished = 0;

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
#if REMOTE_INSERTS
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
    // Frequent Elements below
#elif TOPKAPI
    updateTopkapi(localThreadData, key, increment, rows_no);
#elif PRIF
    prifUpdate(localThreadData, key, increment);
#endif
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
                uint64_t tick = getticks();  // Latency measurement
                #endif

                #if SPACESAVING
                QpopssQuery(threadData,localThreadData->tid,PHI,&localThreadData->lasttopk, filterMatrix);
                #elif TOPKAPI
                topkapi_query_merge(localThreadData,buckets_no,K,rows_no,numberOfThreads);
                #endif

                #if LATENCY
                localThreadData->latencies[numTopKQueries >= 2000000 ? numTopKQueries % 2000000 : numTopKQueries] = getticks() - tick;
                #endif
                numTopKQueries++;
            }                        
            numOps++;
            #if TOPKAPI || PRIF
            insert(localThreadData,key,1);
            #elif DELEGATION_FILTERS
            serveDelegatedInserts(localThreadData);
            // int old_owner = key - numberOfThreads * libdivide::libdivide_s32_do((uint32_t)key, fastDivHandle);
            int owner = findOwner(key);
            delegateInsert(localThreadData, key, 1, owner, filterMatrix, MAX_FILTER_SUM);
            #elif SINGLE
			insertSingleSS(localThreadData,key);
            #endif
        }
        //If duration is 0 then I only loop once over the input. This is to do accuracy tests.
        if (DURATION == 0){
            break;
        }
    }
    #if PRIF
    startBenchmark = false;
    __sync_fetch_and_add( &threadsFinished, 1);
    #endif
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

/*! \brief Preinserts the data into the sketch. 
 * 
 *  This function is used to preinsert the data into the sketch. 
 *  It is used as a warm-up phase for the benchmark, and is not timed. 
 *  \param localThreadData The thread data structure of the current thread.
*/

void preinsert(threadDataStruct * localThreadData){
    int start,end;
    #if TOPKAPI || PRIF
    start=localThreadData->startIndex;
    end=localThreadData->endIndex;
    startBenchmark = true;
    #else
    start=0;
    end=tuples_no;
    #endif
    #if PRIF
    if (tid == numberOfThreads-1){
        prifMergeThreadWorkPreins(localThreadData);
    }
    else{
        for (int i = start; i < end; i++){
        uint32_t key = (*localThreadData->theData->tuples)[i];
        insert(localThreadData,key,1);
        }
    }
    #else
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
    #if PRIF
    __sync_fetch_and_add( &threadsFinished, 1);
    if (threadsFinished == numberOfThreads-1){
        startBenchmark = false; // stop merging thread. 
        threadsFinished = 0; // reset threadsFinished
        printf("Done with preinsert\n");
    }
    #endif
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
    int oneLessWorkerThread = 0;
    #if PRIF 
    oneLessWorkerThread = 1;
    #endif
    int threadWorkSize = tuples_no /  (numberOfThreads - oneLessWorkerThread);
    localThreadData->startIndex = tid * threadWorkSize;
    localThreadData->endIndex =  localThreadData->startIndex + threadWorkSize; //Stop before you reach that index
    //The last thread gets the rest if tuples_no is not devisible by numberOfThreads
    //(it only matters for accuracy tests)
    if (tid == (numberOfThreads - 1 - oneLessWorkerThread)){
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

    // If we are using PREINSERT, then the elements are inserted once before the benchmark starts, not compatible with accuracy experiments
    #if PREINSERT && !ACCURACY
    preinsert(localThreadData);
    #endif

    // cross barriers, starting the benchmark when all threads are ready
    barrier_cross(&barrier_global);
    barrier_cross(&barrier_started);
    #if PRIF
    if (tid == numberOfThreads-1){
        prifMergeThreadWork(localThreadData, numberOfThreads, PHI);
    }
    else{
        threadWork(localThreadData);
    }
    #else
    threadWork(localThreadData);
    #endif

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
        printf("\nElements: %ld, Truth:%ld, True Positives:%ld",elems.size(),truth.size(),true_positives.size());
        printf("\nPrecision:%f, Recall:%f, AverageRelativeError:%f\n", precision,recall,avg_rel_error );
        printf("\n");
}
void saveAccuracyHistogram(vector<pair<uint32_t,uint32_t>>*sorted_histogram,vector<pair<uint32_t,uint32_t>>*lasttopk,uint64_t sum){
        FILE *fp = fopen("logs/topk_results.txt", "w");
        //N, K,Phi in first row.
        fprintf(fp,"%lu %d %f\n",sum,K,PHI);
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
        (void) !fscanf(file, "%d", &i);
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
    memorysize+=(sizeof(FilterStruct) * (numberOfThreads)*(numberOfThreads)) +  
                    (sizeof(uint32_t) * MAX_FILTER_UNIQUES * 2 *(numberOfThreads)*(numberOfThreads)) + 
                    sizeof(uint64_t)*numberOfThreads + 
                    sizeof(atomic_flag)*(numberOfThreads); 
    // T^2 filters + size of keys and values arrays +  T * counters + flags
    #endif
    #endif
    fprintf(fp,"%d %d\n",memorysize,numcounters);
    fclose(fp);
}


int main(int argc, char **argv)
{
    int DIST_TYPE;
    double DIST_PARAM, DIST_SHUFF;

    int runs_no;

   if ((argc != 19) && (argc != 20))
    {
        printf("Usage: sketch_compare.out dom_size tuples_no buckets_no rows_no DIST_TYPE DIST_PARAM DIST_DECOR runs_no num_threads querry_rate duration(in sec, 0 means one pass over the data), (optional) input_file_name \n");
        exit(1);
    }
    int use_real_data = 0;
    char input_file_name[1024];
    //Histogram of data distribution:
    vector<uint32_t> *histogram = new vector<uint32_t>(MAX_HISTOGRAM_SIZE,0);
    vector<uint32_t> *input_data = new vector<uint32_t>();
    input_data->reserve(MAX_HISTOGRAM_SIZE);

    if (argc==20){
        strcpy(input_file_name,argv[19]);
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

    BETA = atof(argv[18]);

    //srand((unsigned int)time((time_t *)NULL));
    srand(0);

    //generate the two relations
    Relation *r1 = new Relation(tuples_no, tuples_no);
    r1->tuples = input_data;
    tuples_no = r1->tuples_no;

    precomputeMods(numberOfThreads);
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
        #if TOPKAPI
        for (int th_i = 0; th_i < rows_no; ++th_i){
            allocate_sketch( &th_local_sketch[i * rows_no + th_i], buckets_no);
        }
    #endif
    }

    filterMatrix = (FilterStruct *) calloc((numberOfThreads)*(numberOfThreads), sizeof(FilterStruct));
    for (int thread = 0; thread< (numberOfThreads)*(numberOfThreads); thread++){
        filterMatrix[thread].filterCount=0;
        filterMatrix[thread].filterFull=false;
        filterMatrix[thread].filter_id = (uint32_t *) calloc(MAX_FILTER_UNIQUES,sizeof(uint32_t));
        filterMatrix[thread].filter_count = (uint32_t *) calloc(MAX_FILTER_UNIQUES,sizeof(uint32_t));
        for (int j=0; j< MAX_FILTER_UNIQUES; j++){
            filterMatrix[thread].filter_id[j]=-1;
        }
    }
    initThreadData(cmArray,r1,MAX_FILTER_SUM,MAX_FILTER_UNIQUES,TOPK_QUERY_RATE,tuples_no,numberOfThreads,th_local_sketch,cm_cw2b,BETA);
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
    QpopssQuery(threadData, numberOfThreads-1, PHI,&threadData[numberOfThreads-1].lasttopk, filterMatrix);
    #elif TOPKAPI
    topkapi_query_merge(&threadData[numberOfThreads-1],buckets_no,num_topk,rows_no,numberOfThreads);
    #elif PRIF
    prifQuery(&(threadData[numberOfThreads-1]));
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
    }

    #if LATENCY
    const double clockspeed_hz = 3601000000.0; // clockspeed of the server :3066775000.0
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
    vector<pair<uint32_t,uint32_t>> lasttopk=threadData[numberOfThreads-1].lasttopk; // Query at the end of the stream
    printAccuracyResults(&sorted_histogram,&lasttopk,streamsize);
    saveAccuracyHistogram(&sorted_histogram,&lasttopk,streamsize);
    //saveMemoryConsumption(&threadData[numberOfThreads-1],numberOfThreads);
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

    delete r1;

    return 0;
}
