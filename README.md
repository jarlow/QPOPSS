The Delegation Space-Saving algorithm
----------------------------------
#Master thesis project in Computer Science aimed at developing and evaluating a multithreaded algorithm for finding frequent elements in a data stream.
Fulltext pdf of the associated report is available [here](https://gupea.ub.gu.se/handle/2077/69761). [Slides](https://1drv.ms/p/s!AoGCrA99762dlJ0tFWP2XnC7Y9bFkQ?e=R4UNUR).
#Heavily
Influenced by:

 - "Delegation sketch: a parallel design with support for fast and accurate concurrent operations", Charalampos Stylianopoulos, Ivan Walulya, Magnus Almgren, Olad Landsiedel, Marina Papatriantafilou, EuroSys'20, Heraklion, Greece, [pdf](https://dl.acm.org/doi/abs/10.1145/3342195.3387542), [github](https://github.com/mpastyl/DelegationSketch)

and 

 - "Efficient Computation of Frequent and Top-k Elements in Data Streams", Ahdmed Metwally, Divyakant Agrawal and Amr El Abbadi,ICDT'05,Edinburgh, UK, [pdf](https://link.springer.com/chapter/10.1007%2F978-3-540-30570-5_27), [full-length pdf](https://cs.ucsb.edu/sites/default/files/documents/2005-23.pdf)

Instructions
----------------------------------
To build:

```
mkdir bin
cd src
make
```

To generate raw result-data and plots:
```
mkdir logs plots
./execution_scripts/freq_elems_performance.sh
./execution_scripts/freq_elems_accuracy.sh
python3 plotting/absolute_error_accuracy.py
python3 plotting/memory_consumption.py
python3 plotting/accuracy.py
python3 plotting/performance.py
```
The python scripts for generating plots are written in python3 and depend on the packages seaborn,numpy and pandas

Main source files
------------------------------
src/cm_benchmark.cpp contains the benchmark entry point, input and data structure creation, as well as the main processing loop.
Also contains the main routines of the Delegation Space-Saving algorithm.

src/lossycount.cpp and include/lossycount.h contains the insertion and frequent elements query implementations of the Space-Saving algorithm (taken from this [website](http://hadjieleftheriou.com/frequent-items/)). The implementation used in this project is the Space-Saving Heap (SSH) implementation mentioned in this [paper](http://hadjieleftheriou.com/papers/vldb08-2.pdf), which is referred to as LCU in the code.

include/filter.h contains operations regarding filters, used by the algorithm
