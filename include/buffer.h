#include <utility>
#include <cstdint>
#define BUFSIZE 100000000
typedef std::pair<uint32_t,uint32_t> buffer_t;
int getitem(buffer_t *itemp, volatile int *startBenchmark);
int putitem(buffer_t item); 
int bufferinit(void);
int release_producers(void);
int release_consumer(void);
bool buffercontains(buffer_t *itemp);
int getnumitems();
int getnumslots();
