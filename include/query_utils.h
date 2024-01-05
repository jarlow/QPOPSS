#pragma once
#include <vector>
#include "filter.h"
#include "hash_utils.h"

int shouldQuery(const threadDataStruct *ltd){
    return (my_random(&(ltd->seeds[0]), &(ltd->seeds[1]), &(ltd->seeds[2])) % 1000);
}
int shouldTopKQuery(const threadDataStruct *ltd){
    return ((my_random(&(ltd->seeds[0]), &(ltd->seeds[2]), &(ltd->seeds[1]))) % 1000000);
}

bool sortbysecdesc(const pair<uint32_t,uint32_t> &a,
                   const pair<uint32_t,uint32_t> &b)
{
       return a.second>b.second;
}

/*! \brief This function adds the elements in Delegation Filters to the query-result vector.
 * \param tid The thread id of the thread that is calling this function. 
 * \param it The iterator to the query-result vector. 
 * \param end The end of the query-result vector. 
 * \param numberOfThreads The number of threads that are running. 
 * \param filterMatrix The matrix of filters that are used to add the results. 
 */
void addDelegationFilterCounts(const int tid, 
            vector<pair<uint32_t,uint32_t>>::iterator &it, 
            const vector<pair<uint32_t,uint32_t>>::const_iterator &end, 
            const int numberOfThreads, 
            const FilterStruct *filterMatrix)
{
    auto start = it;
    for (int i=0; i<numberOfThreads; i++){ // for each thread
        const FilterStruct* filter = &(filterMatrix[i * (numberOfThreads) + tid]);
        while (it != end){ // for each element in the result vector
            for (int k=0; k<filter->filterCount; k++){ // for each element in the filter
                if (it->first == filter->filter_id[k]){ // if filter element matches result element, add the count
                    it->second += filter->filter_count[k];
                }
            }
            ++it;
        }
        it = start;
    };
}

