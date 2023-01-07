#pragma once
#include "thread_data.h"
#include "utils.h"
#include "query_utils.h"
#include "buffer.h" // Circularbuffer implementation
#include "owfrequent.h" // OWFrequent implementation

void prifUpdate(threadDataStruct *localThreadData, uint32_t key, uint32_t increment){
    OWF_Update(localThreadData->owf, key, increment);
}


void prifQuery(threadDataStruct *localThreadData, int numberOfThreads, double PHI){
    uint32_t streamsize = 0;
    for (int i=0;i<numberOfThreads;i++){
        streamsize+=threadData[i].owf->N_i;
    }
    OWF_Output(localThreadData->owf,streamsize*(PHI-(1/(float)COUNTING_PARAM)),localThreadData->lasttopk);
    std::sort(localThreadData->lasttopk.data(), localThreadData->lasttopk.data()+localThreadData->lasttopk.size(),sortbysecdesc);
}

/* !
    * \brief The task carried out by the merging thread.
    * \param localThreadData The thread data structure of the merging thread.
*/
void prifMergeThreadWork(threadDataStruct *localThreadData, int numberOfThreads, double PHI){
    int numTopKQueries = 0;
    std::pair<uint32_t,uint32_t> res;
    while (!startBenchmark)
    {
    }
    while (startBenchmark)
    {
        getitem(&res,&startBenchmark); // Get a frequency increment from the shared buffer
        OWF_Update_Merging_Thread(localThreadData->owf,res.first, res.second); // update merging owf with the frequency increment
        if (shouldTopKQuery(localThreadData) < TOPK_QUERY_RATE)
        {   
            prifQuery(localThreadData, numberOfThreads, PHI);
            numTopKQueries++;
        }
    }
    int c = 0;
    while (buffercontains(&res)){ // empty buffer
        c++;
        OWF_Update_Merging_Thread(localThreadData->owf,res.first, res.second); // update merging owf with the frequency increment
    }
    printf("Emptying buffer, entries %d\n",c);
    localThreadData->numTopKQueries=numTopKQueries;
}

void prifMergeThreadWorkPreins(threadDataStruct *localThreadData){
    std::pair<uint32_t,uint32_t> res;
    while (!startBenchmark)
    {
    }
    while (startBenchmark)
    {
        getitem(&res,&startBenchmark); // Get a frequency increment from the shared buffer
        OWF_Update_Merging_Thread(localThreadData->owf,res.first, res.second); // update merging owf with the frequency increment
    }
    int c =0;
    while (buffercontains(&res)){ // empty buffer
        c++;
        OWF_Update_Merging_Thread(localThreadData->owf,res.first, res.second); // update merging owf with the frequency increment
    }
    printf("Emptying buffer, entries %d\n",c);
}