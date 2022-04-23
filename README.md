The Delegation Space-Saving algorithm
----------------------------------
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
