#ifndef _FREQITEMS
#define _FREQITEMS


//trying out different maps, hence lots of imports
//#include "intset.h"
//#include <libcuckoo/cuckoohash_map.hh>
//#include "ConcurrentMap_Leapfrog.h"
//#include <xenium/harris_michael_hash_map.hpp>
//#include <xenium/reclamation/new_epoch_based.hpp>
//#include <xenium/reclamation/lock_free_ref_count.hpp>
//#include <xenium/reclamation/epoch_based.hpp>
#include <unordered_map>
//#include <queue>
//#include <boost/unordered_map.hpp>
//#include <tbb/concurrent_unordered_map.h>
#include "libdivide.h"


class CountingAlgorithm
{   
    protected:
      //DS_TYPE * frequentItems;
      //libcuckoo::cuckoohash_map<uint64_t, uint64_t> frequentItems;
      //std::unordered_map<uint64_t, uint64_t> frequentItems;
      //xenium::harris_michael_hash_map<uint64_t,uint64_t,xenium::policy::reclaimer<xenium::reclamation::hazard_pointer<>>> frequentItems;
      //boost::unordered::unordered_map<uint64_t, uint64_t> frequentItems;
      //tbb::concurrent_unordered_map<uint64_t,uint64_t> frequentItems;
      //std::priority_queue<Item, std::vector<Item>, Compare > q;
    public:
    //tbb::concurrent_unordered_map<uint64_t,uint64_t> frequentItems;
    //xenium::harris_michael_hash_map<uint64_t,uint64_t,xenium::policy::reclaimer<xenium::reclamation::lock_free_ref_count<>>> frequentItems;
      virtual int size()=0;
      virtual void Insert(uint64_t N, uint64_t key, uint64_t amount,int filterSum)=0;
      virtual void printkeys()=0;
      virtual std::vector<std::pair<uint64_t, uint64_t> > get_topK(int k)=0;
      //virtual xenium::harris_michael_hash_map<uint64_t,uint64_t,xenium::policy::reclaimer<xenium::reclamation::hazard_pointer<>>> get_hashmap()=0;
      virtual ~CountingAlgorithm();
};


class alignas(64) LossyCounting : public CountingAlgorithm
{
  protected:
    double w;
    int old_delta;
    struct libdivide::libdivide_u32_t fast_d;
    void prune();
  public:
    LossyCounting(int w);
    virtual ~LossyCounting();
    virtual int size();
    virtual void Insert(uint64_t N, uint64_t key, uint64_t amount,int filterSum);
    virtual void printkeys();
    std::vector<std::pair<uint64_t, uint64_t> > get_topK(int k);
    //virtual xenium::harris_michael_hash_map<uint64_t,uint64_t,xenium::policy::reclaimer<xenium::reclamation::hazard_pointer<>>> get_hashmap();
};

//
#endif
