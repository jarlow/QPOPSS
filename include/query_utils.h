#pragma once
#include <vector>
#include "filter.h"
#include "hash_utils.h"

int shouldQuery(threadDataStruct *ltd){
    return (my_random(&(ltd->seeds[0]), &(ltd->seeds[1]), &(ltd->seeds[2])) % 1000);
}
int shouldTopKQuery(threadDataStruct *ltd){
    return ((my_random(&(ltd->seeds[0]), &(ltd->seeds[2]), &(ltd->seeds[1]))) % 1000000);
}

bool sortbysecdesc(const pair<uint32_t,uint32_t> &a,
                   const pair<uint32_t,uint32_t> &b)
{
       return a.second>b.second;
}

void addDelegationFilterCounts(int tid, vector<pair<uint32_t,uint32_t>>::iterator &it, const vector<pair<uint32_t,uint32_t>>::const_iterator &end, int numberOfThreads, FilterStruct* filterMatrix){
    auto start = it;
    for (int i=0; i<numberOfThreads; i++){
        FilterStruct* filter = &(filterMatrix[i * (numberOfThreads) + tid]);
        int advancements=0;
        while (it != end){
            for (int k=0; k<filter->filterCount; k++){
                if (it->first == filter->filter_id[k]){ // if matching id, add count to the result. 
                    it->second += filter->filter_count[k];
                    //printf("Found match for %d, adding %d to count\n",it->first,filter->filter_count[k]);
                }
            }
            ++it;
        }
        it = start;
    };
}

