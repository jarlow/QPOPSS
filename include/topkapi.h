#pragma once
#include "thread_data.h"
#include "LossyCountMinSketch.h" // Lossy Count-Min Sketch implementation

#include <map>

void updateTopkapi(threadDataStruct* localThreadData, uint32_t key, uint32_t increment, int rows_no){
    for (int i = 0; i < rows_no; ++i){
        update_sketch( &(localThreadData->th_local_sketch[localThreadData->tid * rows_no + i]), key, localThreadData->randoms,i);
    }
}

/*! \brief Allocate a LossySketch
 * \param sketch The LossySketch to allocate
 * \param range The range each subsketch, i.e. number of rows in each subsketch
*/
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

/*! \brief Deallocate a LossySketch
 * \param sketch The LossySketch to deallocate
*/
void deallocate_sketch( LossySketch* sketch )
{
  free((*sketch).identity);
  free((*sketch).lossyCount);
}

/*! 
    * \brief Merges local subsketches to create a final result-sketch containing the result of the query
    * \param localThreadData thread state
    * \param range number of columns in each subsketch
    * \param rows_no number of rows in each subsketch
    * \param numberOfThreads number of subsketches
*/
void topkapi_query_merge(threadDataStruct * const localThreadData, 
            const int range, 
            const int num_topk, 
            const int rows_no, 
            const int numberOfThreads){
        auto ntopk=std::max(1,num_topk);
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
        int frac_epsilon=ntopk*10;
        int* is_heavy_hitter = (int* )malloc(range*sizeof(int));
        int threshold = (int) ((range/ntopk) - 
                            (range/frac_epsilon));

        for (i = 0; i < range; ++i){
            is_heavy_hitter[i] = false;
            for (j = 0; j < rows_no; ++j){
                if ( j == 0){
                    elem = merged->identity[i];
                    count = merged->lossyCount[i];
                    if (count >= threshold)
                    {
                        is_heavy_hitter[i] = true;
                    }
                } 
                else {
                    id = threadData->randoms[j]->element(elem);
                if ((merged[j].identity[id] !=  elem) )
                {
                    continue;
                } else if (merged[j].lossyCount[id] >= threshold)
                {
                    is_heavy_hitter[i] = true;
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
                (i < ntopk) && (rit != topk_words.rend()); 
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

/*! \brief Alternate query method, Query topkapi for the top k elements without merging. 
 *  \param sketch the sketch to query
 *  \param K the number of elements to return
 *  \param phi threshold value
 *  \param v the vector to store the results in
 */

void topkapi_query(const threadDataStruct *threadDataArray, 
        const int K, 
        const float phi, 
        vector<pair<uint32_t,uint32_t>> * const v)
{
    v->clear();
    std::unordered_map<uint32_t,uint32_t> res;
    for (int t=0;t < numberOfThreads;t++){
        threadDataArray[t].topkapi_instance->Query_Local_Sketch(&res);
    }
    for (auto elem : res){
        v->push_back(elem);
    }
    std::sort(v->data(), v->data()+v->size(), sortbysecdesc);

    /* slice away elements that are not part of the top k */
    v->erase(v->end()-(v->size()-K),v->end());
}