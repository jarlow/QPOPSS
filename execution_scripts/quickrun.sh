#!/bin/bash
compile=$1
type=$2
if [ $type = "" ]; then
    type="throughput"
fi

if [ "$compile" = "1" ]; then 
    cd src || exit
    make clean
    make -j$(nproc) "freq_elems_${2}"
    cd ../
fi

#echo colors
RED='\033[0;31m'
NC='\033[0m' # No Color

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

num_counters_prif (){
    eps=$1
    T=$2
    beta=$3
    res=$(echo "1/(($T/(1+$T)) * ($eps - $beta))" | bc -l)
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
num_thr='24'

rows=4

universe_size="100000000"
stream_size="100000000"
skew="1.25"
num_seconds=0
EPSILONratio="0.5"
BETAratio="0.1"


#Real Data
#filename="/home/victor/git/Delegation-Space-Saving/words.txt"
#filename="/home/victor/git/Delegation-Space-Saving/caida_dst_ip.txt"
filename="/home/victor/git/Delegation-Space-Saving/datasets/flows_dirA.txt"
#filename="/home/victor/git/Delegation-Space-Saving/caida_dst_port.txt"
#Synthetic data 
filename="/home/victor/git/Delegation-Space-Saving/datasets/zipf_${skew}_${stream_size}.txt"
topk_rates="0"
queries="0"
phi="0.00001"
MAX_FILTER_SUM="1000"
K=1000
MAX_FILTER_UNIQUES="16"
versions="cm_spacesaving_deleg_min_heap_${2}"
#versions="cm_spacesaving_deleg_min_max_heap_${2}" #cm_spacesaving_deleg_min_heap_${2}" #cm_topkapi_accuracy" #"cm_spacesaving_deleg cm_spacesaving_deleg_maxheap cm_topkapi" #cm_topkapi_accuracy #cm_spacesaving_deleg_accuracy cm_spacesaving_deleg_maxheap_accuracy
for version in $versions; do
    eps=$(echo "$phi*$EPSILONratio" | bc -l)
    eps=0$eps
    beta=$(echo "$eps*$BETAratio" | bc -l)
    beta=0$beta
    dss_counters=$(num_counters_deleg "$eps" "$skew" "$num_thr")
    dss_counters=$(( dss_counters*num_thr ))

    new_columns=4 #$(num_counters_topkapi "$dss_counters" "$MAX_FILTER_UNIQUES" "$num_thr" "$rows" "$skew")
    echo "K ${K}"
    if [[ "$version" == *"prif"* ]]; then
        calgo_param=$(num_counters_prif "$eps" "$num_thr" "$beta")
    else
        calgo_param=$(num_counters_deleg "$eps" $skew $((num_thr)))
    fi
    echo "buckets per thread: ${new_columns}"
    echo "counters per thread: ${calgo_param}"
    echo -e "${RED} ${version} ${NC}"
    echo "$universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $beta $filename"
    ./bin/$version.out $universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $beta $filename
done

eps=$(echo "$phi*$EPSILONratio" | bc -l)
eps=0$eps
beta=$(echo "$eps*$BETAratio" | bc -l)
beta=0$beta
calgo_param=$(num_counters_single "$eps" "$skew")
num_thr="1"
echo "spacesaving single"
echo "counters: ${calgo_param}"
new_columns="100"
K=1000
echo "$K"
echo "$universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM 64 $beta $filename"
./bin/cm_spacesaving_single_min_max_heap_accuracy.out $universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds "$calgo_param" $topk_rates $K $phi $MAX_FILTER_SUM 64 $beta $filename
