#include <pthread.h>
#include "countingalgorithms.h"
#include <map>
#include <cassert>
#include <math.h>
#include <algorithm> 

using namespace std;

CountingAlgorithm::~CountingAlgorithm(){};

LossyCounting::LossyCounting(int w) {
    this->fast_d = libdivide::libdivide_u32_gen(w);
    this->w=1/(double)w; // save inverse so we can multiply later!
    this->old_delta=0;
}
LossyCounting::~LossyCounting(){
    w=0;
}
// keep track of frequent items using Lossy Counting, from this paper by Manku, Motwani: https://www.vldb.org/conf/2002/S10P03.pdf
void LossyCounting::Insert(uint64_t N, uint64_t  key, uint64_t amount,int filterSum){
    //printf("inhere\n");
    //uint64_t delta = round(N*this->w);
    uint32_t delta = libdivide_u32_do(N,&this->fast_d);
    uint32_t deltaFuture = libdivide_u32_do(N+filterSum,&this->fast_d);
    //printf("%d\n",delta);
    //uint64_t delta = round(N*w);
    //uint64_t delta = 500;

    //printf("%d\n",deltaPlus1-delta);
    auto got = this->frequentItems.find(key);
    if (got == this->frequentItems.end()){
        //printf("amount:%d\n",amount);
        if (amount+delta > deltaFuture){ //We're in the process of inserting a filter that will make our future delta become deltaFuture. If 
        //if (amount > 1){    // heuristic optimization: if the item only occurs once in delegationfilter, it is probably outlier.  
        //this->frequentItems[key]=delta+amount;
        this->frequentItems.emplace(key,delta+amount);
        }
    } 
    else{
        //this->frequentItems[key]+=amount;
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
    auto end = this->frequentItems.end();
    while (it != end) {
        if (it->second < this->old_delta) {
            //assert(this->frequentItems.size() >= 1);
            it = this->frequentItems.unsafe_erase(it);
        }
        else{
            ++it; 
        }
    }
    
}

int LossyCounting::size(){
    return 1; //this->frequentItems.size();
}
void LossyCounting::printkeys(){
}
bool sortbysec(const pair<int,int> &a,
              const pair<int,int> &b)
{
    return (a.second > b.second);
}
std::vector<std::pair<uint64_t, uint64_t> > LossyCounting::get_topK(int k){
    vector<pair<uint64_t, uint64_t> > v;
    auto m = (this->frequentItems);
    v.reserve(m.size());
    std::for_each(m.begin(),m.end(),
            [&v](const std::pair<uint64_t,uint64_t>& entry) 
            { v.push_back(entry); });
    sort(v.begin(), v.end(), sortbysec);
    int min = std::min(k,(int)v.size());
    return std::vector<std::pair<uint64_t, uint64_t>>(v.begin(),v.begin()+min);
}
/*
std::vector<pair<uint64_t, uint64_t> > CountingAlgorithm::get_topK(int k){
    vector<pair<uint64_t, uint64_t> > v(0);
    auto m = (this->frequentItems);
    v.reserve(m.size());
    std::for_each(m.begin(),m.end(),
            [&v](const std::pair<uint64_t,uint64_t>& entry) 
            { v.push_back(entry); });
    sort(v.begin(), v.end(), sortbysec);
    return std::vector<pair<uint64_t, uint64_t>>(v.begin(),v.begin()+k);
}
*/
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

