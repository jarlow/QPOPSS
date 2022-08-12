#!/bin/bash
compile=$1
if [ "$compile" = "1" ]; then 
    cd src || exit
    make clean
    make freq_elems_performance
    #make freq_elems_accuracy
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


num_counters_topkapi(){
    dss_counters_tot=$1
    df_size=$2
    T=$3
    rows=$4
    a=$5
    if (( $(echo "$a <= 1" | bc -l) )); then
        a="1"
    fi
    aThRoot=$(echo "e( l($dss_counters_tot/$T)/$a )" | bc -l)
    res=$(echo "(16*$aThRoot + $T*2*$df_size + 9*$T) / (2*$rows)" | bc -l)
    res=${res%.*}
    echo $res
}
num_thr='8'

rows=4

universe_size="100000000"
stream_size="100000000"
skew="0.75"
num_seconds=4
EPSILONratio="0.1"


#Real Data
#filename="/home/victor/git/Delegation-Space-Saving/words.txt"
#filename="/home/victor/git/Delegation-Space-Saving/caida_dst_ip.txt"
#filename="/home/victor/git/Delegation-Space-Saving/datasets/flows_dirA.txt"
#filename="/home/victor/git/Delegation-Space-Saving/caida_dst_port.txt"
#Synthetic data 
filename="/home/victor/git/Delegation-Space-Saving/datasets/zipf_${skew}_${stream_size}.txt"
topk_rates="1000"
queries="0"
phi="0.0001"
MAX_FILTER_SUM="1000"
K=1000
MAX_FILTER_UNIQUES="16"
versions="cm_spacesaving_deleg_min_max_heap" #cm_topkapi_accuracy" #"cm_spacesaving_deleg cm_spacesaving_deleg_maxheap cm_topkapi" #cm_topkapi_accuracy #cm_spacesaving_deleg_accuracy cm_spacesaving_deleg_maxheap_accuracy
for version in $versions; do
    eps=$(echo "$phi*$EPSILONratio" | bc -l)
    eps=0$eps
    dss_counters=$(num_counters_deleg "$eps" "$skew" "$num_thr")
    dss_counters=$(( dss_counters*num_thr ))

    new_columns=$(num_counters_topkapi "$dss_counters" "$MAX_FILTER_UNIQUES" "$num_thr" "$rows" "$skew")
    echo "K ${K}"
    calgo_param=$(num_counters_deleg "$eps" $skew $((num_thr)))
    echo "buckets per thread: ${new_columns}"
    echo "counters per thread: ${calgo_param}"
    echo "${version}"
    echo "$universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $filename"
    ./bin/$version.out $universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $filename
    #output=$(./bin/$version.out $universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $filename)
    #echo "$output"
done

#eps=$(echo "$phi*$EPSILONratio" | bc -l)
#eps=0$eps
#calgo_param=$(num_counters_single "$eps" "$skew")
#num_thr="1"
#echo "spacesaving single"
#echo "counters: ${calgo_param}"
#new_columns="100"
#K=1000
#echo "$K"
#echo "$universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM 64 $filename"
#./bin/cm_spacesaving_single_maxheap.out $universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds "$calgo_param" $topk_rates $K $phi $MAX_FILTER_SUM 64 $filename