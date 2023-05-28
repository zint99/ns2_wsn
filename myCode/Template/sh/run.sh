#!/bin/bash
# 每次产生一个新的场景，对AODV和AOMDV分别跑十组发包率仿真
# 产生十组cbr数据流,批量执行实验
i=1
#  每次重新产生场景
setdest setdest -n 50 -p 300 -M 0.0000001 -t 300 -x 800 -y 800 > sc-n50-p300-M0-t300-x800-y800

while (test $i -lt 11)
do
    # 生成数据流场景
    rate=$(echo "3.0 + 3.0*($i-1)" | bc)
    ns ../util/cbrgen.tcl -type cbr -nn 50 -seed 1 -mc 10 -rate $rate > cbr$rate
    # 运行仿真
    echo 'aodv仿真开始'
    ns aodv.tcl cbr$rate
    echo '\n'
    echo 'aomdv仿真开始'
    ns aomdv.tcl cbr$rate
    # 计算平均延迟并写入 CSV 文件
    gawk -f avg_delay.awk trace/aodvcbr$rate.tr >> delay_aodv.csv
    gawk -f avg_delay.awk trace/aomdvcbr$rate.aomdv.tr >> delay_aomdv.csv
    echo '本轮仿真结束\n'
    i=$((i+1))
done