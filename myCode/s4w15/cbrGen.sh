#!/bin/bash
# 产生十组cbr文件
i=1
while (test $i -lt 11)
do
#生成数据流场景
    rate=$(echo "3.0 + 3.0*($i-1)" | bc)
    ns ../util/cbrgen.tcl -type cbr -nn 50 -seed 1 -mc 10 -rate $rate > ../cbr/cbr$rate
    i=$((i+1))
done