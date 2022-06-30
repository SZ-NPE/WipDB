#!/bin/bash
# echo fb0-=0-= | sudo -S bash -c 'echo 800000 > /proc/sys/fs/file-max'
echo 800000 > /proc/sys/fs/file-max # 内核可以打开的最大的文件句柄数，默认 6571781
ulimit -n 800000 # 修改打开文件描述符的最大值，默认 1024

OPS=8000000 # 8M request (单线程) * 4
# NUM=1200000000 # 12 亿 load 单线程 Load 120G
NUM=120000000 # 12G
BLOOM_BITS=16
CACHE=8388608
VALUE=100

data_path="/mnt/shunzi/test-data/wipdb"
data_path_bk="/mnt/shunzi/test-data/wipdb-bk"
cd ..

# echo "==========wipdb============"
# rm -rf $data_path/kv_ycsb
# rm -rf $data_path_bk/kv_ycsb
# echo 1 > /proc/sys/vm/drop_caches
# cgexec -g memory:kv64 ./kv/release/tests/db/kv_bench --cache_size=$CACHE --db=$data_path/kv_ycsb   --threads=4 --open_files=40000  --num=$NUM --range=$NUM  --reads=2000000 --partition=100 --write_buffer_size=2097152  --batch_size=1000  --stats_interval=10000000  --bloom_bits=$BLOOM_BITS --low_pool=3 --high_pool=3 --value_size=$VALUE  --direct_io=false --histogram=true    --log=true --skiplistrep=false  --benchmarks=load,overwrite,stats           --writes=10000000 --ycsb_ops_num=$OPS --seek_nexts=100  --use_existing_db=false --print_wa=false --log_dio=true --hugepage=true | tee kv1B_ycsb_load.log
# mv $data_path/kv_ycsb $data_path_bk

# echo "==========rocksdb============"
# rm -rf $data_path/rocks_ycsb
# rm -rf $data_path_bk/rocks_ycsb
# echo 1 > /proc/sys/vm/drop_caches
# cgexec -g memory:kv64 ./rocksdb/release/db_bench     --cache_size=$CACHE --db=$data_path/rocks_ycsb --threads=4 --open_files=40000  --num=$NUM --range=$NUM  --reads=2000000                                              --batch_size=1000  --stats_interval=10000000  --bloom_bits=$BLOOM_BITS --max_background_jobs=7    --value_size=$VALUE                    --histogram=true  --disable_wal=false              --benchmarks=load,overwrite,stats,levelstats --writes=10000000 --ycsb_ops_num=$OPS  --seek_nexts=100  --use_existing_db=false  --stats_per_interval=1000000 | tee rocks1B_ycsb_load.log
# mv $data_path/rocks_ycsb $data_path_bk

# 运行 pebblesdb 之前需要修改 ulimit 配置才能正常运行
# echo "==========pebblesdb============"
# rm -rf $data_path/peb_ycsb
# rm -rf $data_path_bk/peb_ycsb
# echo 1 > /proc/sys/vm/drop_caches
# echo "==========pebblesdb running============"
# cgexec -g memory:kv64 ./pebblesdb/release/db_bench  --cache_size=$CACHE --db=$data_path/peb_ycsb  --threads=4 --open_files=40000   --num=$NUM  --range=$NUM  --reads=2000000                                              --batch_size=1000  --stats_interval=10000000  --bloom_bits=$BLOOM_BITS --bg_threads=6             --value_size=$VALUE                    --histogram=true  --log=true                       --benchmarks=load,overwrite,stats          --writes=10000000 --ycsb_ops_num=$OPS  --seek_nexts=100  --use_existing_db=false --write_buffer_size=67108864    | tee peb1B_ycsb_load.log
# mv $data_path/peb_ycsb $data_path_bk

echo "==========leveldb============"
rm -rf $data_path/level_ycsb
rm -rf $data_path_bk/level_ycsb
echo 1 > /proc/sys/vm/drop_caches
cgexec -g memory:kv64 ./leveldb/release/db_bench    --cache_size=$CACHE --db=$data_path/level_ycsb --threads=4 --open_files=40000   --num=$NUM --range=$NUM  --reads=2000000                                              --batch_size=1000  --stats_interval=10000000  --bloom_bits=$BLOOM_BITS                            --value_size=$VALUE                    --histogram=true   --log=true                      --benchmarks=load,overwrite,stats          --writes=10000000 --ycsb_ops_num=$OPS  --seek_nexts=100  --use_existing_db=false --print_wa=true --write_buffer_size=67108864  --max_file_size=67108864 | tee level1B_ycsb_load.log
mv $data_path/level_ycsb $data_path_bk