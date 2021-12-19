#include <stdlib.h>
#include <string.h>
#include "prng.h"
#include "LossyCountMinSketch.h"

/* This function updates the local sketch 
 * based on input word
 */ 

void update_sketch( LossySketch* _sketch,
                    uint32_t element,
                    Xi ** xi_bucket, int index )
{
    int bucket = (int)xi_bucket[index]->element(element);
    int count = _sketch->lossyCount[bucket];
    uint32_t identity = _sketch->identity[bucket];
    if (count == -1)
    { /* if the counter is empty */
        _sketch->identity[bucket]=element;
        _sketch->lossyCount[bucket]=1;
    }
    else
    { 
        if (identity == element){
            ++(_sketch->lossyCount[bucket]);
        }
        else
        { 
            --(_sketch->lossyCount[bucket]);
            if (_sketch->lossyCount[bucket] == 0)
            {
                _sketch->identity[bucket]=element;
                _sketch->lossyCount[bucket]=1;
            }
        }
    }
}

/* This function merges thread local sketches to
 * create the final sketch for a node
 * Note: it is used only when multi-threaded
 * execution happens
 */
void local_merge_sketch(LossySketch* final, 
                        LossySketch*   LCMS,
                         const unsigned num_local_copies,
                         const unsigned num_hash_func,
                         const unsigned hash_func_index )
{
  uint32_t word[num_local_copies];
  int count[num_local_copies];
  unsigned i, j, k, diff_words;
  int max_selected;
  uint32_t current_word;
  int max_count;
  unsigned range = LCMS[0]._b;

  for (i = 0; i < range; ++i)
  {
    word[0] = LCMS[hash_func_index].identity[i];
    count[0] = LCMS[hash_func_index].lossyCount[i];
    diff_words = 1;
    for (j = 1; j < num_local_copies; ++j)
    {
      current_word = LCMS[j*num_hash_func+hash_func_index].identity[i];
      for ( k = 0; k < diff_words; ++k)
      {
        if (current_word == word[k] &&
            LCMS[j*num_hash_func+hash_func_index].lossyCount[i] != (-1))
        {
          /* if same word */
          count[k] += LCMS[j*num_hash_func+hash_func_index].lossyCount[i];
          break;
        }
      }
      if (k == diff_words)
      {
        word[diff_words] = current_word;
        count[diff_words] = LCMS[j*num_hash_func+hash_func_index].lossyCount[i];
        diff_words++;
      }
    }
    max_count = -1;
    max_selected = 0;
    k = 0;
    for (j = 0; j < diff_words; ++j)
    {
      if (count[j] != (-1))
      {
        if (max_selected)
        {
          if (count[j] > max_count)
          {
            max_count = (count[j] - max_count);
            k = j;
          } else {
            max_count -= count[j];
          }
        } else {
          max_count = count[j];
          k = j;
          max_selected = 1;
        }
      }
    }
    if (k != 0)
    {
        word[0]=word[k];
    }
    final[hash_func_index].lossyCount[i] = max_count;
    final[hash_func_index].identity[i] = current_word;
  }
}