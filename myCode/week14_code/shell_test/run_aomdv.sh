#!/bin/bash
# 产生十组cbr文,批量执行实验
i=1
while (test $i -lt 11)
do
    # 生成数据流场景
    rate=$(echo "3.0 + 3.0*($i-1)" | bc)
    # ns ../util/cbrgen.tcl -type cbr -nn 50 -seed 1 -mc 10 -rate $rate > cbr$rate
    # 运行仿真
    ns aomdv.tcl cbr$rate
    # 计算平均延迟并写入 CSV 文件
    gawk -f avg_delay.awk trace/cbr$rate.tr >> delay_aomdv.csv
    echo '本轮仿真结束\n'
    i=$((i+1))
done