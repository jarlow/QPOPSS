#
# Compare normal delegationsketch vs deleg + lossy vs single threaded lossy 
#
RUN_ON_ITHACA=2
num_thr='24'
compile=$1

buckets=300 #use 800 for odysseus, 512 for ithaca
rows=8

universe_size=100000
stream_size=10000000
zipf_skew_param=2.5
num_seconds=6

calgo_param="7000" #bucket size of 1000 for lossy or t value of 1000.


#versions="cm_shared cm_local_copies cm_hybrid cm_remote_inserts cm_remote_inserts_filtered cm_shared_filtered cm_local_copies_filtered cm_augmented_sketch cm_delegation_filters cm_delegation_filters_with_linked_list"
#versions="cm_shared cm_local_copies cm_augmented_sketch cm_delegation_filters cm_delegation_filters_with_linked_list"
#versions="cm_delegation_filters_with_linked_list"
versions="lossy_deleg_filter cm_delegation_filters_with_linked_list"
#versions="lossy"


if [ "$compile" = "1" ]; then 
    cd src
    make clean
    make topk_compare ITHACA=$RUN_ON_ITHACA
    cd ../
fi
#./bin/$versions.out 

num_reps=`seq 1`

query_rates="0"
for version in $versions
do
    echo $version
    for queries in $query_rates
    do
        new_columns=$((($buckets*$rows*4 - $num_thr*64)/($rows*4))) 
        ./bin/$version.out $universe_size $stream_size $new_columns $rows 1 $zipf_skew_param 0 1 $num_thr $queries $num_seconds $calgo_param
    done
done
echo "lossy single"
        new_columns=$((($buckets*$rows*4 - $num_thr*64)/($rows*4))) 
        ./bin/lossy_deleg_filter.out $universe_size $stream_size $new_columns $rows 1 $zipf_skew_param 0 1 1 $queries $num_seconds $calgo_param

