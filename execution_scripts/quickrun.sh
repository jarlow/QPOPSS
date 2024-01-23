#!/bin/bash
CURR_DIR=$(dirname "$0")
REPO_DIR=$(readlink -f "${CURR_DIR}/..")
source "$(dirname "$0")/helper_functions.sh"

SHOULD_COMPILE=$1
TYPE=$2

if [ "$SHOULD_COMPILE" = "1" ]; then
    compile "$REPO_DIR/src" "$TYPE"
fi

#echo colors
RED='\033[0;31m'
NC='\033[0m' # No Color

num_thr='24'
rows=4
universe_size="1000000"
stream_size="1000000"
skew="1.25"
num_seconds=10
EPSILONratio="0.1"
BETAratio="0.1"
phi="0.00001"
eps=$(echo "$phi*$EPSILONratio" | bc -l)
eps=0$eps
beta=$(echo "$eps*$BETAratio" | bc -l)
beta=0$beta

#Real Data
#filename="${REPO_DIR}/words.txt"
#filename="${REPO_DIR}/caida_dst_ip.txt"
filename="${REPO_DIR}/datasets/flows_dirA.txt"
#filename="${REPO_DIR}/caida_dst_port.txt"

#Synthetic data 
filename="${REPO_DIR}/datasets/zipf_${skew}_${stream_size}.txt"

topk_rates="0"
queries="0"
MAX_FILTER_SUM="1000"
K=1000
MAX_FILTER_UNIQUES="16"
versions="" #"cm_spacesaving_deleg_min_max_heap_${TYPE}"
#versions="prif"
#versions="cm_spacesaving_deleg_min_max_heap_${TYPE}" #cm_spacesaving_deleg_min_heap_${TYPE}" #cm_topkapi_accuracy" #"cm_spacesaving_deleg cm_spacesaving_deleg_maxheap cm_topkapi" #cm_topkapi_accuracy #cm_spacesaving_deleg_accuracy cm_spacesaving_deleg_maxheap_accuracy
for version in $versions; do
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
    "${REPO_DIR}/bin/$version.out" $universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds "$calgo_param" $topk_rates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES "$beta" "$filename"
done

calgo_param=$(num_counters_single "$eps" "$skew")
num_thr="1"
echo "spacesaving single"
echo "counters: ${calgo_param}"
new_columns="100"
K=1000
echo "$K"
echo "$universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM 64 $beta $filename"
"${REPO_DIR}/bin/cm_spacesaving_single_min_max_heap_${TYPE}.out" $universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds "$calgo_param" $topk_rates $K $phi $MAX_FILTER_SUM 64 "$beta" "$filename"
