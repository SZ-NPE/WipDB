#!/bin/bash

for txt in $(ls ycsb-thread*.trace)
do
    cat $txt >> ycsb.txt
    rm $txt
done

cat ycsb.txt | uniq -c | wc -l