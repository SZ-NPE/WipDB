# hugepage
# 大页面 16K
# echo 16384 > /proc/sys/vm/nr_hugepages


# cgroup
# cgroup 限制资源使用，创建不同的资源组

# kv4 限制 4G 内存，swappiness 表示最大限度使用物理内存
mkdir /sys/fs/cgroup/memory/kv4
echo 4G > /sys/fs/cgroup/memory/kv4/memory.limit_in_bytes
echo 0 > /sys/fs/cgroup/memory/kv4/memory.swappiness

mkdir /sys/fs/cgroup/memory/kv16
echo 16G > /sys/fs/cgroup/memory/kv16/memory.limit_in_bytes
echo 0 > /sys/fs/cgroup/memory/kv16/memory.swappiness

mkdir /sys/fs/cgroup/memory/kv64
echo 64G > /sys/fs/cgroup/memory/kv64/memory.limit_in_bytes
echo 0 > /sys/fs/cgroup/memory/kv64/memory.swappiness

mkdir /sys/fs/cgroup/memory/kv80
echo 77G > /sys/fs/cgroup/memory/kv80/memory.limit_in_bytes
echo 0 > /sys/fs/cgroup/memory/kv80/memory.swappiness

# 更改文件所属用户
# chown -R hanson:hanson /sys/fs/cgroup/memory/kv*


# # mount 挂载目录
# # 写日志存储的位置 /mnt/ssd
# mount /dev/sdb /mnt/ssd # where the store write log
# # 数据存储的位置 /mnt/nvm
# mount /dev/nvme0n1 /mnt/nvm # where the store keep all the records

# # turn off the journaling
# # 关闭两个盘上的文件系统的日志
# tune2fs -O ^has_journal /dev/nvme0n1
# tune2fs -O ^has_journal /dev/sdb

# # 设置 CPU 到性能模式
# # set CPU in performance mode
# cmd='-g performance'
# MAX_CPU=$((`nproc --all` - 1))
# for i in $(seq 0 $MAX_CPU); do
#     echo "Changing CPU " $i " with parameter "$cmd;
#     cpufreq-set -c $i $cmd ;
# done