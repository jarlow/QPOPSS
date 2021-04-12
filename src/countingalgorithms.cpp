//#include "barrier.h"
//#include <urcu.h>
//#include <urcu/rculfhash.h>	
#include <pthread.h>
#include "countingalgorithms.h"
#include <map>
#include <cassert>

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
void LossyCounting::Insert(uint64_t N, uint64_t  key, uint64_t amount){
    //printf("inhere\n");
    uint64_t delta = N/this->w;
    auto got = this->frequentItems.find(key);
    if (got == this->frequentItems.end()){
        if (amount > 1){ // heuristic optimization: if the item only occurs once in delegationfilter, it is probably outlier.  
            this->frequentItems.emplace(key,delta+amount);
        }
    } 
    else{
        got->second+=amount;
    }
    
    /*
    uint64_t val;
    if (frequentItems.find(key, val)){
        frequentItems.update(key,val+amount);
        //val+=amount;
    }
    else{
        frequentItems.insert(key,delta+amount);
    }*/

    if (this->old_delta != delta){
        this->old_delta=delta;
        this->prune();
    }
}// iterate over items and remove if value < delta
void LossyCounting::prune(){ 
    //printf("pruning!!\n");

    /*
    auto lt = this->frequentItems.lock_table();
    auto lt_it = lt.begin();
    while (lt_it != lt.end()) {
        if (lt_it->second < this->old_delta) {
            lt.erase(lt_it->first);
        }
        lt_it++;
    }

    */
    
    auto it = this->frequentItems.begin();
    while (it != this->frequentItems.end()) {
        if (it->second < this->old_delta) {
            assert(this->frequentItems.size() >= 1);
            it = this->frequentItems.unsafe_erase(it);
        }
        else{
            ++it; 
        }
    }
    
}

int LossyCounting::size(){
    return this->frequentItems.size();
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

