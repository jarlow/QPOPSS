#!/bin/bash
CURR_DIR=$(dirname "$0")
REPO_DIR=$(readlink -f "${CURR_DIR}/..")
source $(dirname $0)/helper_functions.sh

compile=$1

if [ "$compile" = "1" ]; then 
    compile "$REPO_DIR/src" "accuracy"
fi

declare -A topk
regex="SKEW:[[:space:]]*([0-9]\.{0,1}[1-9]{0,2})[[:space:]]*NUM TOPK:[[:space:]]*([0-9]+)[[:space:]]*PHI:[[:space:]]*(0\.[0-9]+)[[:space:]]*FILEPATH:[[:space:]]*.*\/datasets\/(.*)\.txt"

### Which experiments to run?
vsdfsdfu=false #examine impact of df_s and df_u
vsN=true #examine impact of N
vs=true # examine impact of query rate and phi.
vt=true # Vary threads

### Sources of data
declare -A datasets
datasets[" "]=""
datasets["flows_dirA"]="/home/victor/git/Delegation-Space-Saving/datasets/flows_dirA.txt"
datasets["flows_dirB"]="/home/victor/git/Delegation-Space-Saving/datasets/flows_dirB.txt"

EPSILONratio="0.1"
BETAratio="0.1" #favorable to PRIF, use 0.5 when testing throughput

query_rates="0"
topk_rates="0"
threads="24"
K="55555"
MAX_FILTER_SUMS="1000"
MAX_FILTER_UNIQUES="16"
rows=4

reps=2
num_reps=$(seq $reps)

skew_rates="0.5 0.75 1 1.25 1.5 1.75 2 2.25 2.5 2.75 3"

phi="0.0001"
MAX_FILTER_SUMS="100 1000 10000 100000"
MAX_FILTER_UNIQUES="16 32 64 128"
streamlengths="100000000"
beta="0.1"
versions="cm_spacesaving_deleg_min_max_heap_accuracy"
## Vary Skew, df_s and df_u
echo "------ Vary Skew, df_s and df_u------"
if [ "$vsdfsdfu" = true ] ; then 
    mkdir -p logs/accuracy/vsdfsdfu
    for version in $versions
    do 
        echo "$version"
        for skew in $skew_rates
        do
            num_thr="24"
            eps=$(echo "$phi*$EPSILONratio" | bc -l)
            eps=0$eps
            beta=$(echo "$eps*$BETAratio" | bc -l)
            beta=0$beta
            calgo_param=$(num_counters_deleg "$eps" "$skew" $num_thr)
            echo "skew: $skew"
            for MAX_FILTER_SUM in $MAX_FILTER_SUMS
            do
                for MAX_FILTER_UNIQUE in $MAX_FILTER_UNIQUES
                do
                    for N in $streamlengths
                    do
                        echo "N: $N"
                        for rep in $num_reps
                        do
                            filename="/home/victor/git/Delegation-Space-Saving/datasets/zipf_${skew}_${N}.txt"
                            echo "rep: $rep"
                            new_columns=4
                            output=$(./bin/"$version".out "$N" "$N" $new_columns $rows 1 "$skew" 0 1 $num_thr 0 0 "$calgo_param" $topk_rates $K $phi "$MAX_FILTER_SUM" "$MAX_FILTER_UNIQUE" "$beta" "$filename")
                            echo "$N" "$N" $new_columns $rows 1 "$skew" 0 1 $num_thr 0 0 "$calgo_param" $topk_rates $K $phi "$MAX_FILTER_SUM" "$MAX_FILTER_UNIQUE"
                            echo "$output"
                            echo "$output" | grep -oP 'Precision:\d.\d+, Recall:\d.\d+, AverageRelativeError:\d.\d+' -a --text >> logs/accuracy/vsdfsdfu/var_skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_dfsdfu_accuracy.log
                            echo "$output" >> logs/accuracy/vsdfsdfu/var_skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_dfsdfu_accuracy_info.log
                        done
                    done
                done
            done
        done
    done

fi

phi="0.0001"
MAX_FILTER_SUMS="1000"
MAX_FILTER_UNIQUES="16"
streamlengths="1000000 10000000 100000000"

#versions="cm_spacesaving_deleg_min_max_heap_accuracy cm_spacesaving_single_min_max_heap_accuracy cm_topkapi_accuracy" #"cm_topkapi_accuracy cm_spacesaving_deleg_min_max_heap_accuracy cm_spacesaving_single_min_max_heap_accuracy"
versions="prif_accuracy"
## Vary Skew and N
echo "------Vary Skew and N------"
if [ "$vsN" = true ] ; then 
    mkdir -p logs/accuracy/vsN
    for version in $versions
    do 
        echo "$version"
        for skew in $skew_rates
        do
            eps=$(echo "$phi*$EPSILONratio" | bc -l)
            eps=0$eps
            beta=$(echo "$eps*$BETAratio" | bc -l)
            beta=0$beta
            if [[ "$version" == *"single"* ]]; then 
                num_thr="1"
                calgo_param=$(num_counters_single "$eps" "$skew")
            elif [[ "$version" == *"prif"* ]]; then
                num_thr="24"
                calgo_param=$(num_counters_prif "$eps" "$num_thr" "$beta")
            else
                num_thr="24"
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
            echo "skew: $skew"
            for MAX_FILTER_SUM in $MAX_FILTER_SUMS
            do
                for MAX_FILTER_UNIQUE in $MAX_FILTER_UNIQUES
                do
                    for N in $streamlengths
                    do
                        echo "N: $N"
                        filepath="/home/victor/git/Delegation-Space-Saving/datasets/zipf_${skew}_${N}.txt"
                        for rep in $num_reps
                        do
                            echo "rep: $rep"
                            output=$(./bin/"$version".out "$N" "$N" "$new_columns" $rows 1 "$skew" 0 1 $num_thr 0 0 "$calgo_param" $topk_rates "$K" $phi "$MAX_FILTER_SUM" "$MAX_FILTER_UNIQUE" "$beta" """$filepat""h")
                            echo "$output" | grep -oP 'Precision:\d.\d+, Recall:\d.\d+, AverageRelativeError:\d.\d+' -a --text >> logs/accuracy/vsN/var_skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_varN_accuracy.log
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
                        cp logs/topk_results.txt logs/accuracy/vsN/var_skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_varN_histogram.log
                    done
                done
            done
        done
    done
fi

phis="0.001 0.0002 0.0001"
MAX_FILTER_SUM="1000"
MAX_FILTER_UNIQUE="16"
streamlengths="100000000"
beta="0.1"
#versions="cm_spacesaving_deleg_min_max_heap_accuracy cm_spacesaving_single_min_max_heap_accuracy cm_topkapi_accuracy"
versions="prif_accuracy"
## Vary Skew and phi (query rate has no effect on accuracy)
echo "------ Vary Skew and phi ------"
if [ "$vs" = true ] ; then 
    mkdir -p logs/accuracy/vs
    for version in $versions
    do 
        echo "$version"
        for skew in $skew_rates
        do
            echo "skew: $skew"
            for phi in $phis
            do
                eps=$(echo "$phi*$EPSILONratio" | bc -l)
                eps=0$eps
                beta=$(echo "$eps*$BETAratio" | bc -l)
                beta=0$beta
                if [[ "$version" == *"single"* ]]; then 
                    num_thr="1"
                    calgo_param=$(num_counters_single "$eps" "$skew")
                elif [[ "$version" == *"prif"* ]]; then
                    num_thr="24"
                    calgo_param=$(num_counters_prif "$eps" "$num_thr" "$beta")
                else
                    num_thr="24"
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
                for topk_rates in $query_rates
                do
                    for N in $streamlengths
                    do
                        filepath="/home/victor/git/Delegation-Space-Saving/datasets/zipf_${skew}_${N}.txt"
                        echo "N: $N"
                        for rep in $num_reps
                        do
                            echo "rep: $rep"
                            output=$(./bin/"$version".out "$N" "$N" "$new_columns" $rows 1 "$skew" 0 1 $num_thr 0 0 "$calgo_param" "$topk_rates" "$K" "$phi" $MAX_FILTER_SUM $MAX_FILTER_UNIQUE "$beta" "$filepath")
                            echo "$N $N $new_columns $rows 1 $skew 0 1 $num_thr 0 0 $calgo_param $topk_rates $K $phi $MAX_FILTER_SUM $MAX_FILTER_UNIQUE $filepath"
                            echo "$output"
                            echo "$output" | grep -oP 'Precision:\d.\d+, Recall:\d.\d+, AverageRelativeError:\d.\d+' -a --text >> logs/accuracy/vs/var_skew_"${version}"_${num_thr}_"${skew}"_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_phi_accuracy.log
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
fi

phis="0.001 0.0002 0.0001"
MAX_FILTER_SUM="1000"
MAX_FILTER_UNIQUE="16"
streamlengths="100000000"
threads="4 8 12 16 20 24"
skew="1.25" 
#versions="cm_spacesaving_deleg_min_max_heap_accuracy cm_spacesaving_single_min_max_heap_accuracy cm_topkapi_accuracy"
versions="prif_accuracy"
## Vary threads, and phi
echo "------ Vary Threads and phi------"

if [ "$vt" = true ] ; then
    mkdir -p logs/accuracy/vt
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
                threads="1"
            else
                threads="4 8 12 16 20 24"
            fi
            echo "$version"
            for num_thr in $threads
            do
                echo "skew: $skew"
                for phi in $phis
                do
                    eps=$(echo "$phi*$EPSILONratio" | bc -l)
                    eps=0$eps
                    beta=$(echo "$eps*$BETAratio" | bc -l)
                    beta=0$beta
                    if [[ "$version" == *"single"* ]]; then 
                        calgo_param=$(num_counters_single "$eps" $skew)
                    elif [[ "$version" == *"prif"* ]]; then
                        calgo_param=$(num_counters_prif "$eps" "$num_thr" "$beta")
                    else
                        calgo_param=$(num_counters_deleg "$eps" $skew "$num_thr")
                    fi
                    dss_counters=$(num_counters_deleg "$eps" "$skew" "$num_thr")
                    dss_counters=$(( dss_counters*num_thr ))

                    if [[ "$version" == *"topkapi"* ]]; then
                        K=${topk[${skew}${phi}${dsname}]}
                    else
                        K=999
                    fi
                    new_columns=$(num_counters_topkapi "$dss_counters" "$MAX_FILTER_UNIQUES" "$num_thr" "$rows" "$skew")
                    for topk_rates in $query_rates
                    do
                        for N in $streamlengths
                        do
                            if [[ "$dsname" == "" ]]; then
                                filepath="/home/victor/git/Delegation-Space-Saving/datasets/zipf_${skew}_${N}.txt"
                            fi
                            echo "N: $N"
                            for rep in $num_reps
                            do
                                echo "rep: $rep"
                                #new_columns=$(((buckets*rows*4 - num_thr*64)/(rows*4)))
                                output=$(./bin/"$version".out "$N" "$N" "$new_columns" $rows 1 $skew 0 1 "$num_thr" 0 0 "$calgo_param" "$topk_rates" "$K" "$phi" $MAX_FILTER_SUM $MAX_FILTER_UNIQUE "$beta" "$filepath")
                                echo "$output" | grep -oP 'Precision:\d.\d+, Recall:\d.\d+, AverageRelativeError:\d.\d+' -a --text >> logs/accuracy/vt/var_threads_"${version}"_"${num_thr}""_"${skew}_"${phi}"_"${MAX_FILTER_SUM}"_"${MAX_FILTER_UNIQUE}"_"${N}"_phi"${dsname}"_accuracy.log
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
fi
