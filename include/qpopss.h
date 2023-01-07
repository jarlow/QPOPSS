#include "lossycount.h" // Space-Saving implementation
#include "thread_data.h"
#include "hash_utils.h"
#include "query_utils.h"

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

static inline void delegateInsert(threadDataStruct *localThreadData, unsigned int key, unsigned int increment, int owner, FilterStruct *filterMatrix, int MAX_FILTER_SUM){
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


// Performs a frequent elements query 
void QpopssQuery(threadDataStruct *threadData, int selfId, float phi, vector<pair<uint32_t,uint32_t>> *result, FilterStruct* filterMatrix){
    int numFETotal=0;
    LCL_type* local_spacesaving;
    result->resize(0);
    uint64_t streamsize=0;

    // If single threaded just extract frequent elements from the local Space-Saving instance
    #if SINGLE
    streamsize = threadData[selfId].substreamSize;
    local_spacesaving = threadData[selfId].ss;
    LCL_Output(local_spacesaving,streamsize*phi,result);

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
            int numFE = LCL_Output(local_spacesaving,streamsize*phi,result);
            pthread_mutex_unlock(&threadData[i].mutex);
            
            auto it = result->begin();
            advance(it,numFETotal);
            addDelegationFilterCounts(i,it,result->end(),numberOfThreads,filterMatrix);
            numFETotal += numFE;
            
            bm[i]=true;
            num_complete++;
            serveDelegatedInserts(&threadData[selfId]);
        }
        i=precomputedMods[i+1];
    }
    #endif

    // Sort output
    std::sort(result->data(), result->data()+result->size(),sortbysecdesc);
}