INCLUDE = -I. -I../include
CPP = g++ -O2 -Wno-deprecated -pthread -g -std=c++11 $(INCLUDE) -DITHACA=$(ITHACA)
CPPDEBUG = g++ -O2 -Wno-deprecated -pthread -std=c++11 -g $(INCLUDE)
DEPS = thread_data.h thread_utils.h cm_benchmark.h barrier.h data_generator.h gen_scheme.h range_sum.h relation.h RM7_range_sum.h sampling.h sketches.h utils.h xis.h lossycount.h prng.h rand48.h LossyCountMinSketch.h
BIN = ../bin

all: freq_elems_performance freq_elems_accuracy
freq_elems_performance: cm_spacesaving_deleg.out cm_spacesaving_deleg_maxheap.out cm_spacesaving_single.out cm_spacesaving_single_maxheap.out cm_topkapi.out
freq_elems_accuracy: cm_spacesaving_deleg_accuracy.out cm_spacesaving_deleg_maxheap_accuracy.out cm_spacesaving_single_accuracy.out cm_spacesaving_single_maxheap_accuracy.out cm_topkapi_accuracy.out

sketch_sampling:relation.o xis.o sketches.o sampling.o sketch_sampling.o
	$(CPP) -lgsl -lgslcblas -o sketch_sampling.out relation.o xis.o sketches.o sampling.o sketch_sampling.o

sketch_sampling.o : sketch_sampling.cpp
	$(CPP) -c sketch_sampling.cpp

sketch_compare:relation.o xis.o sketches.o sketch_compare.o
	$(CPP) -o sketch_compare.out relation.o xis.o sketches.o sketch_compare.o

sketch_compare.o : sketch_compare.cpp 
	$(CPP) -c sketch_compare.cpp

# cm_benchmark.o : cm_benchmark.cpp $(DEPS)
# 	$(CPP) -c cm_benchmark.cpp 

# cm_benchmark:relation.o xis.o sketches.o cm_benchmark.o
# 	$(CPP) -o cm_benchmark.out relation.o xis.o sketches.o cm_benchmark.o

lossycountminsketch.o : LossyCountMinSketch.cpp
	$(CPP) -c LossyCountMinSketch.cpp -o lossycountminsketch.o

relation.o : relation.cpp
	$(CPP) -c relation.cpp

xis.o : xis.cpp 
	$(CPP) -c xis.cpp

sketches.o : sketches.cpp 
	$(CPP) -c sketches.cpp

sampling.o : sampling.cpp
	$(CPP) -c sampling.cpp

lossycount.o : lossycount.cpp
	$(CPP) -c lossycount.cpp 

lossycount_maxheap.o : lossycount.cpp
	$(CPP) -c lossycount.cpp -D MAXHEAP=1 -o lossycount_maxheap.o

prng.o : prng.cpp
	$(CPP) -c prng.cpp

rand48.o : rand48.cpp
	$(CPP) -c rand48.cpp

cm_shared.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D SHARED_SKETCH=1 

cm_local_copies.o: cm_benchmark.cpp
	$(CPP) -c cm_benchmark.cpp -D LOCAL_COPIES=1 

cm_hybrid.o: cm_benchmark.cpp
	$(CPP) -c cm_benchmark.cpp -D HYBRID=5 

cm_remote_inserts.o: cm_benchmark.cpp
	$(CPP) -c cm_benchmark.cpp -D REMOTE_INSERTS=1 

cm_remote_inserts_mpsc.o: cm_benchmark.cpp
	$(CPP) -c cm_benchmark.cpp -D USE_MPSC=1 

#Topkapi versions
cm_topkapi.o : cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D TOPKAPI=1 -D DELEGATION_FILTERS=0 -D USE_LIST_OF_FILTERS=0

cm_topkapi_accuracy.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D TOPKAPI=1 -D ACCURACY=1 -D DELEGATION_FILTERS=0 -D USE_LIST_OF_FILTERS=0

#Filtered versions

# frequent with delegationsketch
spacesaving.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D SPACESAVING=1 -D DELEGATION_FILTERS=1 -D USE_LIST_OF_FILTERS=1 

spacesaving_accuracy.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D SPACESAVING=1 -D DELEGATION_FILTERS=1 -D USE_LIST_OF_FILTERS=1 -D ACCURACY=1
	
spacesaving_single.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D SPACESAVING=1 -D SINGLE=1 

spacesaving_single_accuracy.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D SPACESAVING=1 -D SINGLE=1 -D ACCURACY=1

cm_remote_inserts_filtered.o: cm_benchmark.cpp
	$(CPP) -c cm_benchmark.cpp -D REMOTE_INSERTS=1 -D USE_FILTER=1

cm_shared_filtered.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D SHARED_SKETCH=1 -D USE_FILTER=1

cm_local_copies_filtered.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D LOCAL_COPIES=1 -D USE_FILTER=1

cm_augmented_sketch.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D AUGMENTED_SKETCH=1

cm_delegation_filters.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D DELEGATION_FILTERS=1

cm_delegation_filters_with_linked_list.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D DELEGATION_FILTERS=1 -D USE_LIST_OF_FILTERS=1
#--------------------

cm_shared.out:relation.o xis.o sketches.o cm_shared.o 
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o

cm_local_copies.out:relation.o xis.o sketches.o cm_local_copies.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o 

cm_hybrid.out:relation.o xis.o sketches.o cm_hybrid.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o 

cm_remote_inserts.out:relation.o xis.o sketches.o cm_remote_inserts.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o 

cm_remote_inserts_mpsc.out:relation.o xis.o sketches.o cm_remote_inserts_mpsc.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o 

#Filtered versions
cm_remote_inserts_filtered.out:relation.o xis.o sketches.o cm_remote_inserts_filtered.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o 

cm_shared_filtered.out:relation.o xis.o sketches.o cm_shared_filtered.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o 

cm_local_copies_filtered.out:relation.o xis.o sketches.o cm_local_copies_filtered.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o 

cm_augmented_sketch.out:relation.o xis.o sketches.o cm_augmented_sketch.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o 

cm_delegation_filters.out:relation.o xis.o sketches.o cm_delegation_filters.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o 

cm_delegation_filters_with_linked_list.out:relation.o xis.o sketches.o cm_delegation_filters_with_linked_list.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o 

## Counting algorithms
cm_spacesaving_single_maxheap.out : relation.o xis.o sketches.o spacesaving_single.o lossycount_maxheap.o prng.o rand48.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o lossycount_maxheap.o prng.o rand48.o

cm_spacesaving_deleg_maxheap.out: relation.o xis.o sketches.o spacesaving.o lossycount_maxheap.o prng.o rand48.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o lossycount_maxheap.o prng.o rand48.o

cm_spacesaving_deleg.out: relation.o xis.o sketches.o spacesaving.o lossycount.o prng.o rand48.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o lossycount.o prng.o rand48.o

cm_spacesaving_single.out : relation.o xis.o sketches.o spacesaving_single.o lossycount.o prng.o rand48.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o lossycount.o prng.o rand48.o

cm_spacesaving_deleg_accuracy.out: relation.o xis.o sketches.o spacesaving_accuracy.o lossycount.o prng.o rand48.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o lossycount.o prng.o rand48.o

cm_spacesaving_single_accuracy.out : relation.o xis.o sketches.o spacesaving_single_accuracy.o lossycount.o prng.o rand48.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o lossycount.o prng.o rand48.o

cm_spacesaving_single_maxheap_accuracy.out : relation.o xis.o sketches.o spacesaving_single_accuracy.o lossycount_maxheap.o prng.o rand48.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o lossycount_maxheap.o prng.o rand48.o

cm_spacesaving_deleg_maxheap_accuracy.out: relation.o xis.o sketches.o spacesaving_accuracy.o lossycount_maxheap.o prng.o rand48.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o lossycount_maxheap.o prng.o rand48.o

##Topkapi
cm_topkapi.out: relation.o xis.o sketches.o cm_topkapi.o lossycount.o prng.o rand48.o lossycountminsketch.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o lossycount.o prng.o rand48.o lossycountminsketch.o

cm_topkapi_accuracy.out: relation.o xis.o sketches.o cm_topkapi_accuracy.o lossycount.o prng.o rand48.o lossycountminsketch.o
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_benchmark.o lossycount.o prng.o rand48.o lossycountminsketch.o

clean:
	rm -f *.out
	rm -f *.o
	rm -f *~
	rm -f ../bin/*