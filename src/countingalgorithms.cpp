//#include "barrier.h"
//#include <urcu.h>
//#include <urcu/rculfhash.h>	
#include <pthread.h>
#include "countingalgorithms.h"
#include <vector>
#include <map>

using namespace std;

CountingAlgorithm::~CountingAlgorithm(){};

LossyCounting::LossyCounting(int w) {
    this->w=w;
    this->old_delta=0;
}
LossyCounting::~LossyCounting(){
    w=0;
}
// keep track of frequent items, from this paper by Manku, Motwani: https://www.vldb.org/conf/2002/S10P03.pdf
void LossyCounting::Insert(uint64_t N, uint64_t w, uint64_t  key, uint64_t amount){
    uint64_t delta = N/this->w;
    std::unordered_map<uint64_t,uint64_t>::iterator got = this->frequentItems.find(key);
    if (got == this->frequentItems.end()){
        //if (amount > 1){
            this->frequentItems.emplace(key,delta+amount);
        //}
    } 
    else{
        got->second+=amount;
    }
    if (this->old_delta != delta){
        this->prune(delta);
        this->old_delta=delta;
    }
}
// iterate over items and remove if value < delta
void LossyCounting::prune(int delta){ 
    auto it = this->frequentItems.begin();
    while (it != this->frequentItems.end()) {
        if (it->second < delta) {
            assert(this->frequentItems.size() >= 1);
            it = this->frequentItems.erase(it);
        }
        else{
            ++it; 
        }
    }
}

int LossyCounting::size(){
    return this->frequentItems.size();
}
void LossyCounting::GetItems(int k,cds_lfht_iter* iter){
}
void LossyCounting::printkeys(){
}
/*
int main(){
    //rcu_register_thread();
    LossyCounting ls = LossyCounting(1000);
    printf("\n\n created\n\n");
    ls.Insert(400,50,51,4);
    ls.Insert(400,50,52,2);
    ls.Insert(400,50,53,1);
    ls.Insert(400,50,54,100);
    ls.Insert(400,50,51,50);
    ls.Insert(400,50,51,50);
    ls.Insert(400,50,51,50);
    printf("this is size:%d \n",ls.size());
    return 0;
}
*/

