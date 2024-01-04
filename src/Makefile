INCLUDE = -I. -I../include -I../include/apache-data-sketches
CPP = g++ -O2 -Wno-deprecated -pthread -g -std=c++11 $(INCLUDE) -DITHACA=$(ITHACA)
CPPDEBUG = g++ -O2 -Wno-deprecated -pthread -std=c++11 -g $(INCLUDE)
BIN = ../bin

all: freq_elems_throughput freq_elems_accuracy
freq_elems_throughput: cm_spacesaving_deleg_min_heap_throughput.out cm_spacesaving_deleg_min_max_heap_throughput.out cm_spacesaving_single_min_heap_throughput.out cm_spacesaving_single_min_max_heap_throughput.out cm_topkapi_throughput.out
freq_elems_accuracy: cm_spacesaving_deleg_min_heap_accuracy.out cm_spacesaving_deleg_min_max_heap_accuracy.out cm_spacesaving_single_min_heap_accuracy.out cm_spacesaving_single_min_max_heap_accuracy.out cm_topkapi_accuracy.out
freq_elems_prif: prif.out
freq_elems_prif_accuracy: prif_accuracy.out
generate_data: generate_synthetic_data.out

sketch_sampling: relation.o xis.o sketches.o sampling.o sketch_sampling.o
	$(CPP) -lgsl -lgslcblas -o sketch_sampling.out relation.o xis.o sketches.o sampling.o sketch_sampling.o

sketch_sampling.o: sketch_sampling.cpp
	$(CPP) -c sketch_sampling.cpp

sketch_compare:relation.o xis.o sketches.o sketch_compare.o
	$(CPP) -o sketch_compare.out relation.o xis.o sketches.o sketch_compare.o

sketch_compare.o: sketch_compare.cpp 
	$(CPP) -c sketch_compare.cpp

lossycountminsketch.o: LossyCountMinSketch.cpp
	$(CPP) -c LossyCountMinSketch.cpp -o lossycountminsketch.o

owfrequent.o: owfrequent.cpp bufferseminit.o
	$(CPP) -c owfrequent.cpp

relation.o: relation.cpp
	$(CPP) -c relation.cpp

xis.o: xis.cpp 
	$(CPP) -c xis.cpp

bufferseminit.o: bufferseminit.c
	$(CPP) -c bufferseminit.c

sketches.o: sketches.cpp 
	$(CPP) -c sketches.cpp

sampling.o: sampling.cpp
	$(CPP) -c sampling.cpp

lossycount_min_heap.o: lossycount.cpp
	$(CPP) -c lossycount.cpp -o $@

lossycount_min_max_heap.o: lossycount.cpp
	$(CPP) -c lossycount.cpp -D MINMAXHEAP=1 -o $@

prng.o: prng.cpp
	$(CPP) -c prng.cpp

rand48.o: rand48.cpp
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
	$(CPP) -c cm_benchmark.cpp -D TOPKAPI=1 -D DELEGATION_FILTERS=0 -D USE_LIST_OF_FILTERS=0 -o $@

cm_topkapi_accuracy.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D TOPKAPI=1 -D ACCURACY=1 -D DELEGATION_FILTERS=0 -D USE_LIST_OF_FILTERS=0 -o $@

#Filtered versions
spacesaving.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D SPACESAVING=1 -D DELEGATION_FILTERS=1 -D USE_LIST_OF_FILTERS=1 -o $@

spacesaving_accuracy.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D SPACESAVING=1 -D DELEGATION_FILTERS=1 -D USE_LIST_OF_FILTERS=1 -D ACCURACY=1 -o $@
	
spacesaving_single.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D SPACESAVING=1 -D SINGLE=1 -o $@

spacesaving_single_accuracy.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D SPACESAVING=1 -D SINGLE=1 -D ACCURACY=1 -o $@

cm_remote_inserts_filtered.o: cm_benchmark.cpp
	$(CPP) -c cm_benchmark.cpp -D REMOTE_INSERTS=1 -D USE_FILTER=1 -o $@

cm_shared_filtered.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D SHARED_SKETCH=1 -D USE_FILTER=1 -o $@

cm_local_copies_filtered.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D LOCAL_COPIES=1 -D USE_FILTER=1 -o $@

cm_augmented_sketch.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D AUGMENTED_SKETCH=1 -o $@

cm_delegation_filters.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D DELEGATION_FILTERS=1 -o $@

cm_delegation_filters_with_linked_list.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D DELEGATION_FILTERS=1 -D USE_LIST_OF_FILTERS=1 -o $@

prif.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D PRIF=1 -o $@

prif_accuracy.o: cm_benchmark.cpp 
	$(CPP) -c cm_benchmark.cpp -D PRIF=1 -D ACCURACY=1 -o $@
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
cm_spacesaving_single_min_heap_throughput.out : relation.o xis.o sketches.o spacesaving_single.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o spacesaving_single.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 

cm_spacesaving_deleg_min_heap_throughput.out: relation.o xis.o sketches.o spacesaving.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o spacesaving.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 

cm_spacesaving_deleg_min_max_heap_throughput.out: relation.o xis.o sketches.o spacesaving.o lossycount_min_max_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o spacesaving.o lossycount_min_max_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 

cm_spacesaving_single_min_max_heap_throughput.out : relation.o xis.o sketches.o spacesaving_single.o lossycount_min_max_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o spacesaving_single.o lossycount_min_max_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 

cm_spacesaving_deleg_min_max_heap_accuracy.out: relation.o xis.o sketches.o spacesaving_accuracy.o lossycount_min_max_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o spacesaving_accuracy.o lossycount_min_max_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 

cm_spacesaving_single_min_heap_accuracy.out : relation.o xis.o sketches.o spacesaving_single_accuracy.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o spacesaving_single_accuracy.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 

cm_spacesaving_single_min_max_heap_accuracy.out : relation.o xis.o sketches.o spacesaving_single_accuracy.o lossycount_min_max_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o spacesaving_single_accuracy.o lossycount_min_max_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 

cm_spacesaving_deleg_min_heap_accuracy.out: relation.o xis.o sketches.o spacesaving_accuracy.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o spacesaving_accuracy.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 

##Topkapi
cm_topkapi_throughput.out: relation.o xis.o sketches.o cm_topkapi.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_topkapi.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 

cm_topkapi_accuracy.out: relation.o xis.o sketches.o cm_topkapi_accuracy.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o cm_topkapi_accuracy.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 

##Prif 
prif.out: prif.o relation.o xis.o sketches.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o prif.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 

prif_accuracy.out: prif_accuracy.o relation.o xis.o sketches.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 
	$(CPP) -o ${BIN}/$@ relation.o xis.o sketches.o prif_accuracy.o lossycount_min_heap.o prng.o rand48.o lossycountminsketch.o owfrequent.o bufferseminit.o 

##Data generation
generate_synthetic_data.o : generate_synthetic_data.cpp relation.o
	$(CPP) -c generate_synthetic_data.cpp

generate_synthetic_data.out : generate_synthetic_data.o relation.o
	$(CPP) -o ${BIN}/$@ generate_synthetic_data.o relation.o 

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