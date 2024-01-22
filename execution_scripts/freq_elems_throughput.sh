#!/bin/bash
CURR_DIR=$(dirname "$0")
REPO_DIR=$(readlink -f "${CURR_DIR}/..")
source "$(dirname "$0")/helper_functions.sh"

SHOULD_COMPILE=$1

if [ "$SHOULD_COMPILE" = "1" ]; then 
    compile "$REPO_DIR/src" "throughput"
fi

#Topk for each dataset
declare -A topk
regex="SKEW:[[:space:]]*([0-9]\.{0,1}[1-9]{0,2})[[:space:]]*NUM TOPK:[[:space:]]*([0-9]+)[[:space:]]*PHI:[[:space:]]*(0\.[0-9]+)[[:space:]]*FILEPATH:[[:space:]]*.*\/datasets\/(.*)\.txt"

### Synthetic stream parameters
universe_size=10000000
stream_size=10000000
num_seconds=10
N=${stream_size}
rows=4
new_columns=100
queries=0

### Which experiments to run?
vsdfsdfu=false
vs=true
vt=true

### Sources of data
declare -A datasets
datasets[" "]=""
datasets["flows_dirA"]="${REPO_DIR}/datasets/flows_dirA.txt"
datasets["flows_dirB"]="${REPO_DIR}/datasets/flows_dirB.txt"

K="55555"
EPSILONratio="0.1"
BETAratio="0.1" #favorable to PRIF, use 0.1 when testing accuracy
reps=2
num_reps=$(seq $reps)

### Investigate effects of df_u and df_s over skew ###
queries="0"
MAX_FILTER_SUMS="100 1000 10000 100000"
MAX_FILTER_UNIQUES="16 32 64 128"
skew_rates="0.5 0.75 1 1.25 1.5 1.75 2 2.25 2.5 2.75 3"
phis="0.0001"
topkqueriesS="100"
versions="cm_spacesaving_deleg_min_max_heap_throughput"
echo "------ Vary Skew, df_s and df_u ------"
if [ "$vsdfsdfu" = true ] ; then
    mkdir -p "${REPO_DIR}/logs/throughput/vsdfsdfu"
    for version in $versions
    do
        if [[ "$version" == *"single"* ]]; then 
            num_thr="1"
        else
            num_thr="24"
        fi
        echo "$version"
        for MAX_FILTER_UNIQUE in $MAX_FILTER_UNIQUES
        do
            for MAX_FILTER_SUM in $MAX_FILTER_SUMS
            do
                for topkrates in $topkqueriesS
                do
                    for phi in $phis
                    do
                        for skew in $skew_rates
                        do
                            for _ in $num_reps
                            do
                                filename="${REPO_DIR}/datasets/zipf_${skew}_${N}.txt"
                                echo "$universe_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topkrates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUE $beta $filename"
                                output=$("${REPO_DIR}"/bin/"$version".out $universe_size $stream_size $new_columns $rows 1 "$skew" 0 1 $num_thr $queries $num_seconds "$calgo_param" "$topkrates" $K "$phi" "$MAX_FILTER_SUM" "$MAX_FILTER_UNIQUE" "$beta" "$filename") 
                                echo "$output" | grep -oP 'Total processing throughput [+-]?[0-9]+([.][0-9]+)?+' -a --text >> "${REPO_DIR}"/logs/throughput/vsdfsdfu/skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_"${topkrates}"_dfsdfu_throughput.log
                            done
                        done                     
                    done
                done
            done
        done
    done
fi
MAX_FILTER_UNIQUES="16"
MAX_FILTER_SUMS="1000"
phis="0.001 0.0002 0.0001"
topkqueriesS="0 100 200"
#versions="cm_spacesaving_deleg_min_max_heap_throughput cm_topkapi_throughput cm_spacesaving_single_min_max_heap_throughput cm_spacesaving_deleg_min_heap_throughput"
versions="prif_throughput"
## Vary skew with qr and phi
echo "------ Vary skew, query rate and phi------"
if [ "$vs" = true ] ; then
    mkdir -p "${REPO_DIR}logs/throughput/vs"
    for dsname in "${!datasets[@]}"
    do 
        echo "$dsname"
        filepath=${datasets[$dsname]}
        if [[ "$dsname" == " " ]]; then 
            skew_rates="0.5 0.75 1 1.25 1.5 1.75 2 2.25 2.5 2.75 3"
            dsname=""
        else
            skew_rates="0.5"
        fi
        for version in $versions
        do
            if [[ "$version" == *"single"* ]]; then 
                num_thr="1"
            else
                num_thr="24"
            fi
            echo "$version"
            for MAX_FILTER_SUM in $MAX_FILTER_SUMS
            do
                for MAX_FILTER_UNIQUE in $MAX_FILTER_UNIQUES
                do
                    for topkrates in $topkqueriesS
                    do
                        for phi in $phis
                        do
                            for skew in $skew_rates
                            do
                                if [[ "$dsname" == "" ]]; then
                                    filepath="${REPO_DIR}/datasets/zipf_${skew}_${stream_size}.txt"
                                fi
                                eps=$(echo "$phi*$EPSILONratio" | bc -l)
                                eps=0$eps
                                beta=$(echo "$eps*$BETAratio" | bc -l)
                                beta=0$beta
                                if [[ "$version" == *"single"* ]]; then 
                                    calgo_param=$(num_counters_single "$eps" "$skew")
                                elif [[ "$version" == *"prif"* ]]; then
                                    calgo_param=$(num_counters_prif "$eps" "$num_thr" "$beta")
                                else
                                    calgo_param=$(num_counters_deleg "$eps" "$skew" $num_thr)
                                fi
                                dss_counters=$(num_counters_deleg "$eps" "$skew" $num_thr)
                                dss_counters=$(( dss_counters*num_thr ))
                                
                                if [[ "$version" == *"topkapi"* ]]; then
                                    K=${topk[${skew}${phi}${dsname}]}
                                else
                                    K=999
                                fi
                                
                                new_columns=$(num_counters_topkapi "$dss_counters" "$MAX_FILTER_UNIQUES" "$num_thr" "$rows" "$skew")
                                for _ in $num_reps
                                do
                                    echo "$stream_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topkrates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $filepath"
                                    output=$("${REPO_DIR}"/bin/"$version".out $universe_size $stream_size "$new_columns" $rows 1 "$skew" 0 1 $num_thr $queries $num_seconds "$calgo_param" "$topkrates" "$K" "$phi" "$MAX_FILTER_SUM" $MAX_FILTER_UNIQUES "$beta" "$filepath") 
                                    echo "$output" | grep -oP 'Total processing throughput [+-]?[0-9]+([.][0-9]+)?+' -a --text >> "${REPO_DIR}"/logs/throughput/vs/skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_"${topkrates}"_phiqr"${dsname}"_throughput.log
                                    echo "$output"
                                    if [[ "$version" == *"deleg"* ]]; then
                                        if [[ "$output" =~ $regex ]]; then
                                            if [[ "${BASH_REMATCH[4]}" != *"flows"* ]]; then 
                                                BASH_REMATCH[4]=""
                                            fi
                                            topk[${BASH_REMATCH[1]}${BASH_REMATCH[3]}${BASH_REMATCH[4]}]=${BASH_REMATCH[2]}
                                        else
                                            echo "dosnt match :/ "
                                        fi
                                    fi
                                done
                            done                     
                        done
                    done
                done
            done
        done
    done
fi

MAX_FILTER_UNIQUES="16"
MAX_FILTER_SUMS="1000"

phis="0.001 0.0002 0.0001"
topkqueriesS="0 100 200"
#versions="cm_spacesaving_deleg_min_max_heap_throughput cm_topkapi_throughput cm_spacesaving_single_min_max_heap_throughput cm_spacesaving_deleg_min_heap_throughput"
versions="prif_throughput"
threads="4 8 12 16 20 24"
## Vary threads with skew 1.25
echo "------ Vary Threads, query rate and phi------"
if [ "$vt" = true ] ; then
    mkdir -p "${REPO_DIR}/logs/throughput/vt"
    for dsname in "${!datasets[@]}"
    do 
        echo "$dsname"
        filepath=${datasets[$dsname]}
        if [[ "$dsname" == " " ]]; then 
            skew="1.25"
            dsname=""
        else
            skew="0.5"
        fi
        for version in $versions
        do
            if [[ "$version" == *"single"* ]]; then 
                thrs="1"
            else
                thrs=$threads
            fi
            echo "$version"
            for MAX_FILTER_SUM in $MAX_FILTER_SUMS
            do
                for MAX_FILTER_UNIQUE in $MAX_FILTER_UNIQUES
                do
                    for topkrates in $topkqueriesS
                    do
                        for phi in $phis
                        do
                            for num_thr in $thrs
                            do
                                if [[ "$dsname" == "" ]]; then
                                    filepath="${REPO_DIR}/datasets/zipf_${skew}_${stream_size}.txt"
                                fi
                                eps=$(echo "$phi*$EPSILONratio" | bc -l)
                                eps=0$eps
                                beta=$(echo "$eps*$BETAratio" | bc -l)
                                beta=0$beta
                                if [[ "$version" == *"single"* ]]; then 
                                    calgo_param=$(num_counters_single "$eps" "$skew")
                                elif [[ "$version" == *"prif"* ]]; then
                                    calgo_param=$(num_counters_prif "$eps" "$num_thr" "$beta")
                                else
                                    calgo_param=$(num_counters_deleg "$eps" "$skew" "$num_thr")
                                fi
                                dss_counters=$(num_counters_deleg "$eps" "$skew" "$num_thr")
                                dss_counters=$(( dss_counters*num_thr ))

                                if [[ "$version" == *"topkapi"* ]]; then
                                    K=${topk[${skew}${phi}${dsname}]}
                                else
                                    K=999
                                fi
                                new_columns=$(num_counters_topkapi "$dss_counters" "$MAX_FILTER_UNIQUES" "$num_thr" "$rows" "$skew")
                                for _ in $num_reps
                                do
                                    #new_columns=$(((buckets*rows*4 - num_thr*64)/(rows*4))) 
                                    echo "$stream_size $stream_size $new_columns $rows 1 $skew 0 1 $num_thr $queries $num_seconds $calgo_param $topkrates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUES $filepath"
                                    output=$("${REPO_DIR}"/bin/"$version".out $universe_size $stream_size "$new_columns" $rows 1 $skew 0 1 "$num_thr" $queries $num_seconds "$calgo_param" "$topkrates" "$K" "$phi" "$MAX_FILTER_SUM" $MAX_FILTER_UNIQUES "$beta" "$filepath") 
                                    echo "$output" | grep -oP 'Total processing throughput [+-]?[0-9]+([.][0-9]+)?+' -a --text >> "${REPO_DIR}"/logs/throughput/vt/threads_"${version}"_"${num_thr}"_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_"${topkrates}"_phiqr"${dsname}"_throughput.log
                                    echo "$output" 
                                    if [[ "$version" == *"deleg"* ]]; then
                                        if [[ "$output" =~ $regex ]]; then
                                            if [[ "${BASH_REMATCH[4]}" != *"flows"* ]]; then 
                                                BASH_REMATCH[4]=""
                                            fi
                                            topk[${BASH_REMATCH[1]}${BASH_REMATCH[3]}${BASH_REMATCH[4]}]=${BASH_REMATCH[2]}
                                        else
                                            echo "dosnt match :/ "
                                        fi
                                    fi
                                    #echo "$output" | grep -oP 'topk percentage \K([0-9]+\.[0-9]+)' >> logs/skew_${version}_${num_thr}_threads_${topkrates}_queries_${phi}_phi_${MAX_FILTER_SUM}_dfsum_numqueries_final${filename}.log
                                    #echo "$output" | grep -oP 'num Inserts \K([0-9]+)' >> logs/skew_${version}_${num_thr}_threads_${topkrates}_queries_${phi}_phi_${MAX_FILTER_SUM}_dfsum_streamlength_final${filename}.log
                                    #echo "$output" | grep -oP 'num topk \K([0-9]+)' >> logs/skew_${version}_${num_thr}_threads_${topkrates}_queries_${phi}_phi_${MAX_FILTER_SUM}_dfsum_numqueriesabs_final${filename}.log
                                done
                            done                     
                        done
                    done
                done
            done
        done
    done
fi
