echo fb0-=0-= | sudo -S bash -c 'echo 800000 > /proc/sys/fs/file-max'
ulimit -n 800000

OPS=8000000
NUM=1000000000 # 键范围 / 操作数
BLOOM_BITS=16
CACHE=8388608 # 8MB
VALUE=100
THREAD=8 # 8 线程

rm -rf /mnt/nvm/* # 删除对应的数据
rm /mnt/ssd/*.log # 删除对应的日志
cp -R /mnt/ssd/kv_ycsb /mnt/nvm/ # 拷贝对应的初始数据
echo fb0-=0-= | sudo -S fstrim /mnt/ssd # echo fb0-=0-= | sudo -S 输入了对应的用户密码 fb0-=0-= 到 stdin
echo fb0-=0-= | sudo -S fstrim /mnt/nvm
echo fb0-=0-= | sudo -S bash -c 'echo 1 > /proc/sys/vm/drop_caches'
cgexec -g memory:kv64 ./kv/release/tests/db/kv_bench --cache_size=$CACHE --logpath=/mnt/ssd --db=/mnt/nvm/kv_ycsb   --threads=$THREAD --open_files=40000  --num=$NUM --range=$NUM  --reads=2000000 --partition=100 --write_buffer_size=2097152  --batch_size=1000  --stats_interval=1000000  --bloom_bits=$BLOOM_BITS --low_pool=3 --high_pool=3 --value_size=$VALUE  --direct_io=false --histogram=true    --log=true --skiplistrep=true   --benchmarks=loadreverse,ycsba,ycsbb,ycsbc,ycsbf,ycsbd,ycsbe,stats         --ycsb_ops_num=$OPS --seek_nexts=100  --use_existing_db=true  --print_wa=true  --log_dio=true --hugepage=true | tee kv_ycsb.log

# cgexec -g memory:kv64 ./kv/release/tests/db/kv_bench \
#     --cache_size=$CACHE --logpath=/mnt/ssd --db=/mnt/nvm/kv_ycsb   \
#     --threads=$THREAD --open_files=40000  --num=$NUM --range=$NUM  \
#     --reads=2000000 --partition=100 --write_buffer_size=2097152  \
#     --batch_size=1000  --stats_interval=1000000  --bloom_bits=$BLOOM_BITS \
#     --low_pool=3 --high_pool=3 --value_size=$VALUE  --direct_io=false \
#     --histogram=true    --log=true --skiplistrep=true   \
#     --benchmarks=loadreverse,ycsba,ycsbb,ycsbc,ycsbf,ycsbd,ycsbe,stats \
#     --ycsb_ops_num=$OPS --seek_nexts=100  --use_existing_db=true  \
#     --print_wa=true  --log_dio=true --hugepage=true | tee kv_ycsb.log


rm -rf /mnt/nvm/*
rm /mnt/ssd/*.log
cp -R /mnt/ssd/kv_ycsb /mnt/nvm/
echo fb0-=0-= | sudo -S fstrim /mnt/ssd
echo fb0-=0-= | sudo -S fstrim /mnt/nvm
echo fb0-=0-= | sudo -S bash -c 'echo 1 > /proc/sys/vm/drop_caches'
cgexec -g memory:kv64 ./kv/release/tests/db/kv_bench --cache_size=$CACHE --logpath=/mnt/ssd --db=/mnt/nvm/kv_ycsb   --threads=$THREAD --open_files=40000  --num=$NUM --range=$NUM  --reads=2000000 --partition=100 --write_buffer_size=2097152  --batch_size=1000  --stats_interval=1000000  --bloom_bits=$BLOOM_BITS --low_pool=3 --high_pool=3 --value_size=$VALUE  --direct_io=false --histogram=true    --log=true --skiplistrep=false   --benchmarks=loadreverse,ycsba,ycsbb,ycsbc,ycsbf,ycsbd,ycsbe,stats         --ycsb_ops_num=$OPS --seek_nexts=100  --use_existing_db=true  --print_wa=true  --log_dio=true --hugepage=true | tee kv_ycsb_hash.log


rm -rf /mnt/nvm/*
rm /mnt/ssd/*.log
cp -R /mnt/ssd/rocks_ycsb /mnt/nvm/
echo fb0-=0-= | sudo -S fstrim /mnt/ssd
echo fb0-=0-= | sudo -S fstrim /mnt/nvm
echo fb0-=0-= | sudo -S bash -c 'echo 1 > /proc/sys/vm/drop_caches'
cgexec -g memory:kv64 ./rocksdb/release/db_bench    --cache_size=$CACHE  --wal_dir=/mnt/ssd --db=/mnt/nvm/rocks_ycsb --threads=$THREAD --open_files=40000  --num=$NUM --range=$NUM  --reads=2000000                                              --batch_size=1000  --stats_interval=1000000  --bloom_bits=$BLOOM_BITS --max_background_jobs=7    --value_size=$VALUE                    --histogram=true  --disable_wal=false              --benchmarks=loadreverse,ycsba,ycsbb,ycsbc,ycsbf,ycsbd,ycsbe,stats,levelstats  --ycsb_ops_num=$OPS  --seek_nexts=100  --use_existing_db=true   --stats_per_interval=1000000 | tee rocks_ycsb.log


rm -rf /mnt/nvm/*
rm /mnt/ssd/*.log
cp -R /mnt/ssd/peb_ycsb /mnt/nvm/
echo fb0-=0-= | sudo -S fstrim /mnt/ssd
echo fb0-=0-= | sudo -S fstrim /mnt/nvm
echo fb0-=0-= | sudo -S bash -c 'echo 1 > /proc/sys/vm/drop_caches'
cgexec -g memory:kv64 ./pebblesdb/release/db_bench  --cache_size=$CACHE --logpath=/mnt/ssd --db=/mnt/nvm/peb_ycsb  --threads=$THREAD --open_files=40000   --num=$NUM  --range=$NUM  --reads=2000000                                              --batch_size=1000  --stats_interval=1000000  --bloom_bits=$BLOOM_BITS --bg_threads=6             --value_size=$VALUE                    --histogram=true  --log=true                       --benchmarks=loadreverse,ycsba,ycsbb,ycsbc,ycsbf,ycsbd,ycsbe,stats            --ycsb_ops_num=$OPS  --seek_nexts=100  --use_existing_db=true  --write_buffer_size=67108864     | tee peb_ycsb.log


rm -rf /mnt/nvm/*
rm /mnt/ssd/*.log
cp -R /mnt/ssd/level_ycsb /mnt/nvm/
echo fb0-=0-= | sudo -S fstrim /mnt/ssd
echo fb0-=0-= | sudo -S fstrim /mnt/nvm
echo fb0-=0-= | sudo -S bash -c 'echo 1 > /proc/sys/vm/drop_caches'
cgexec -g memory:kv64 ./leveldb/release/db_bench    --cache_size=$CACHE --logpath=/mnt/ssd --db=/mnt/nvm/level_ycsb --threads=$THREAD --open_files=40000   --num=$NUM --range=$NUM  --reads=2000000                                              --batch_size=1000  --stats_interval=1000000  --bloom_bits=$BLOOM_BITS                            --value_size=$VALUE                    --histogram=true   --log=true                      --benchmarks=loadreverse,ycsba,ycsbb,ycsbc,ycsbf,ycsbd,ycsbe,stats            --ycsb_ops_num=$OPS  --seek_nexts=100  --use_existing_db=true  --print_wa=true  --write_buffer_size=67108864  --max_file_size=67108864  | tee level_ycsb.log

