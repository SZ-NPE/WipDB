#!/bin/bash
echo 800000 > /proc/sys/fs/file-max
ulimit -n 800000

OPS=8000000
# NUM=1000000000 # 键范围 / 操作数 10 亿
NUM=100000000 # 1亿 1G
BLOOM_BITS=16
CACHE=8388608 # 8MB
VALUE=100
THREAD=8 # 8 线程

data_path="/mnt/shunzi/test-data/wipdb"
data_path_bk="/mnt/shunzi/test-data/wipdb-bk"

cd ..

# echo "==========wipdb-skip============"
# rm -rf $data_path/kv_ycsb # 删除对应的数据
# cp -R $data_path_bk/kv_ycsb $data_path/ # 拷贝对应的初始数据
# echo 1 > /proc/sys/vm/drop_caches
# cgexec -g memory:kv64 ./kv/release/tests/db/kv_bench --cache_size=$CACHE --db=$data_path/kv_ycsb   --threads=$THREAD --open_files=40000  --num=$NUM --range=$NUM  --reads=2000000 --partition=100 --write_buffer_size=2097152  --batch_size=1000  --stats_interval=1000000  --bloom_bits=$BLOOM_BITS --low_pool=3 --high_pool=3 --value_size=$VALUE  --direct_io=false --histogram=true    --log=true --skiplistrep=true   --benchmarks=loadreverse,ycsba,ycsbb,ycsbc,ycsbf,ycsbd,ycsbe,stats         --ycsb_ops_num=$OPS --seek_nexts=100  --use_existing_db=true  --print_wa=true  --log_dio=true --hugepage=true | tee kv_ycsb.log

echo "==========wipdb-hash============"
rm -rf $data_path/kv_ycsb # 删除对应的数据
cp -R $data_path_bk/kv_ycsb $data_path/ # 拷贝对应的初始数据
echo 1 > /proc/sys/vm/drop_caches
# cgexec -g memory:kv64 ./kv/release/tests/db/kv_bench --cache_size=$CACHE --db=$data_path/kv_ycsb   --threads=$THREAD --open_files=40000  --num=$NUM --range=$NUM  --reads=2000000 --partition=100 --write_buffer_size=2097152  --batch_size=1000  --stats_interval=1000000  --bloom_bits=$BLOOM_BITS --low_pool=3 --high_pool=3 --value_size=$VALUE  --direct_io=false --histogram=true    --log=true --skiplistrep=false   --benchmarks=loadreverse,ycsba,ycsbb,ycsbc,ycsbf,ycsbd,ycsbe,stats         --ycsb_ops_num=$OPS --seek_nexts=100  --use_existing_db=true  --print_wa=true  --log_dio=true --hugepage=true | tee kv_ycsb_hash.log
cgexec -g memory:kv64 ./kv/release/tests/db/kv_bench --cache_size=$CACHE --db=$data_path/kv_ycsb   --threads=$THREAD --open_files=40000  --num=$NUM --range=$NUM  --reads=2000000 --partition=100 --write_buffer_size=2097152  --batch_size=1000  --stats_interval=1000000  --bloom_bits=$BLOOM_BITS --low_pool=3 --high_pool=3 --value_size=$VALUE  --direct_io=false --histogram=true    --log=true --skiplistrep=false   --benchmarks=loadreverse,ycsba,ycsbb,ycsbc,ycsbf,ycsbd,stats         --ycsb_ops_num=$OPS --seek_nexts=100  --use_existing_db=true  --print_wa=true  --log_dio=true --hugepage=true | tee kv_ycsb_hash.log


# echo "==========rocksdb============"
# rm -rf $data_path/rocks_ycsb # 删除对应的数据
# cp -R $data_path_bk/rocks_ycsb $data_path/ # 拷贝对应的初始数据
# echo 1 > /proc/sys/vm/drop_caches
# cgexec -g memory:kv64 ./rocksdb/release/db_bench    --cache_size=$CACHE  --db=$data_path/rocks_ycsb --threads=$THREAD --open_files=40000  --num=$NUM --range=$NUM  --reads=2000000                                              --batch_size=1000  --stats_interval=1000000  --bloom_bits=$BLOOM_BITS --max_background_jobs=7    --value_size=$VALUE                    --histogram=true  --disable_wal=false              --benchmarks=loadreverse,ycsba,ycsbb,ycsbc,ycsbf,ycsbd,ycsbe,stats,levelstats  --ycsb_ops_num=$OPS  --seek_nexts=100  --use_existing_db=true   --stats_per_interval=1000000 | tee rocks_ycsb.log

# echo "==========pebblesdb============"
# rm -rf $data_path/peb_ycsb # 删除对应的数据
# cp -R $data_path_bk/peb_ycsb $data_path/ # 拷贝对应的初始数据
# echo 1 > /proc/sys/vm/drop_caches
# cgexec -g memory:kv64 ./pebblesdb/release/db_bench  --cache_size=$CACHE  --db=$data_path/peb_ycsb  --threads=$THREAD --open_files=40000   --num=$NUM  --range=$NUM  --reads=2000000                                              --batch_size=1000  --stats_interval=1000000  --bloom_bits=$BLOOM_BITS --bg_threads=6             --value_size=$VALUE                    --histogram=true  --log=true                       --benchmarks=loadreverse,ycsba,ycsbb,ycsbc,ycsbf,ycsbd,ycsbe,stats            --ycsb_ops_num=$OPS  --seek_nexts=100  --use_existing_db=true  --write_buffer_size=67108864     | tee peb_ycsb.log

# echo "==========leveldb============"
# rm -rf $data_path/level_ycsb # 删除对应的数据
# cp -R $data_path_bk/level_ycsb $data_path/ # 拷贝对应的初始数据
# echo 1 > /proc/sys/vm/drop_caches
# cgexec -g memory:kv64 ./leveldb/release/db_bench    --cache_size=$CACHE  --db=$data_path/level_ycsb --threads=$THREAD --open_files=40000   --num=$NUM --range=$NUM  --reads=2000000                                              --batch_size=1000  --stats_interval=1000000  --bloom_bits=$BLOOM_BITS                            --value_size=$VALUE                    --histogram=true   --log=true                      --benchmarks=loadreverse,ycsba,ycsbb,ycsbc,ycsbf,ycsbd,ycsbe,stats            --ycsb_ops_num=$OPS  --seek_nexts=100  --use_existing_db=true  --print_wa=true  --write_buffer_size=67108864  --max_file_size=67108864  | tee level_ycsb.log

