#!/bin/bash
compile=$1
if [ "$compile" = "1" ]; then 
    cd src || exit
    make clean
    make freq_elems_accuracy
    cd ../
fi


num_counters_deleg (){
    eps=$1
    a=$2
    T=$3
    if (( $(echo "$a <= 1" | bc -l) )); then
        a="1"
    fi
    res=$(echo "e(l(1/($eps * $T))*(1/$a))" | bc -l)
    res=${res%.*}
    res=$((res+1))
    echo $res
}

num_counters_single (){
    eps=$1
    a=$2
    if (( $(echo "$a <= 1" | bc -l) )); then
        a="1"
    fi
    res=$(echo "e(l(1/($eps))*(1/$a))" | bc -l)
    res=${res%.*}
    res=$((res+1))
    echo $res
}

num_thr='24'

buckets=1024 #use 800 for odysseus, 512 for ithaca
rows=4

universe_size=30000000
stream_size=30000000
skew=1
num_seconds=0
EPSILONratio="0.1"


#Real Data
#filename="/home/victor/git/DelegationSpace-Saving/words.txt"
#filename="/home/victor/git/DelegationSpace-Saving/caida_dst_ip.txt"
#filename="/home/victor/git/DelegationSpace-Saving/caida_dst_port.txt"
#Synthetic data 
filename="" #keep empty if synthetic
topk_rates="10000"
queries="0"
phi="0.001"
K="635"
MAX_FILTER_SUM="1000"
MAX_FILTER_UNIQUES="64"
versions="" #"cm_spacesaving_deleg_maxheap_accuracy" #"cm_spacesaving_deleg cm_spacesaving_deleg_maxheap cm_topkapi" #cm_topkapi_accuracy #cm_spacesaving_deleg_accuracy
for version in $versions
do
    eps=$(echo "$phi*$EPSILONratio" | bc -l)
    eps=0$eps
    calgo_param=$(num_counters_deleg "$eps" $skew $((num_thr)))
    new_columns=${buckets} #$(((buckets*rows*4 - num_thr*64)/(rows*4))) 
    echo "counters per thread: ${calgo_param}"
    echo "$universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $filename"
    ./bin/$version.out $universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $filename
done
echo ""
eps=$(echo "$phi*$EPSILONratio" | bc -l)
eps=0$eps
#calgo_param=$(num_counters_single "$eps" $skew)
calgo_param=$(num_counters_deleg "$eps" $skew $((num_thr)))
calgo_param=$((calgo_param*num_thr))
num_thr="1" 
echo "spacesaving single"
echo "counters: ${calgo_param}"
new_columns=$(((buckets*rows*4 - num_thr*64)/(rows*4))) 
echo "$universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM 64 $filename"
./bin/cm_spacesaving_single_maxheap_accuracy.out $universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM 64 $filename
