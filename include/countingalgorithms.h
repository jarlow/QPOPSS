#ifndef _FREQITEMS
#define _FREQITEMS


//trying out different maps, hence lots of imports
#include "intset.h"
#include <libcuckoo/cuckoohash_map.hh>
//#include "ConcurrentMap_Leapfrog.h"
#include <xenium/harris_michael_hash_map.hpp>
#include <xenium/reclamation/generic_epoch_based.hpp>
#include <unordered_map>


#define DS_CONTAINS(s,k,i)  cds_lfht_lookup(set, key, match, &key, &i)
#define DS_ADD(s,k)         (cds_lfht_add_unique(s, k->key, match, &k->key, &k->node) == &k->node) /* k is a node_t* */
#define DS_REMOVE(s,k)      (cds_lfht_del(s, k) == 0)
#define DS_SIZE(s)          cds_lfht_size(seee)
#define DS_NEW()            cds_lfht_new(maxhtlength, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL)

#define DS_TYPE             cds_lfht_t
#define DS_NODE             node_t


class CountingAlgorithm
{   
    protected:
      //DS_TYPE * frequentItems;
      //libcuckoo::cuckoohash_map<uint64_t, uint64_t> frequentItems;
      std::unordered_map<uint64_t, uint64_t> frequentItems;
    public:
      virtual void GetItems(int k,cds_lfht_iter* iter)=0;
      virtual int size()=0;
      virtual void Insert(uint64_t N, uint64_t w, uint64_t key, uint64_t amount)=0;
      virtual void printkeys()=0;
      virtual ~CountingAlgorithm();
};


class alignas(64) LossyCounting : public CountingAlgorithm
{
  protected:
    int w;
    int old_delta;
    void prune(int delta);
  public:
    LossyCounting(int w);
    virtual ~LossyCounting();
    virtual void GetItems(int k,cds_lfht_iter* iter);
    virtual int size();
    virtual void Insert(uint64_t N, uint64_t w, uint64_t key, uint64_t amount);
    virtual void printkeys();
};
//
#endif
