#!/bin/bash
# 每次产生一个新的场景,测试不同数据流（拥塞程度）对路由协议的影响,批量执行实验
num_scenarios=10

#num_runs控制发包速率
num_runs=6

# 清空结果文件
> ../result/lab2/delay_aodv.csv
> ../result/lab2/delay_aomdv.csv
> ../result/lab2/pdr_aodv.csv
> ../result/lab2/pdr_aomdv.csv
> ../result/lab2/nload_aodv.csv
> ../result/lab2/nload_aomdv.csv
> ../result/lab2/freq_aodv.csv
> ../result/lab2/freq_aomdv.csv

# 外部循环，生成不同的场景文件并进行测试
for ((scenario=1; scenario<=num_scenarios; scenario++))
do
    # 产生场景文件名
    scenario_file="sc-${scenario}-n100-p1000-M10-t2000-x800-y800"

    # 每次重新产生场景
    setdest -n 100 -p 1000 -M 10 -t 2000 -x 800 -y 800 > "$scenario_file"

    # 内部循环，执行十组仿真
    for ((i=1; i<=num_runs; i++))
    do
        # 生成数据流场景
        rate=$(echo "1.0 + 1.0*($i-1)" | bc)
        cbr_file="cbr_${scenario}_${rate}"
        ns ../util/cbrgen.tcl -type cbr -nn 100 -seed 1 -mc 15 -rate $rate > "$cbr_file"

        # 运行仿真
        echo "aodv仿真开始"
        ns aodv.tcl "$cbr_file" "$scenario_file"
        echo -e "\n"
        echo "aomdv仿真开始"
        ns aomdv.tcl "$cbr_file" "$scenario_file"

        # 计算平均延迟并写入 CSV 文件
        gawk -f ../awk/avg_delay.awk "../trace/lab2/aodv${cbr_file}.tr" >> "../result/lab2/delay_aodv.csv"
        gawk -f ../awk/avg_delay.awk "../trace/lab2/aomdv${cbr_file}.tr" >> "../result/lab2/delay_aomdv.csv"
        # PDR
        gawk -f ../awk/getRatio.awk "../trace/lab2/aodv${cbr_file}.tr" >> "../result/lab2/pdr_aodv.csv"
        gawk -f ../awk/getRatio.awk "../trace/lab2/aomdv${cbr_file}.tr" >> "../result/lab2/pdr_aomdv.csv"
        # load
        gawk -f ../awk/N_load_aodv.awk "../trace/lab2/aodv${cbr_file}.tr" >> "../result/lab2/nload_aodv.csv"
        gawk -f ../awk/N_load_aomdv.awk "../trace/lab2/aomdv${cbr_file}.tr" >> "../result/lab2/nload_aomdv.csv"
        # PDR
        gawk -f ../awk/freq.awk "../trace/lab2/aodv${cbr_file}.tr" >> "../result/lab2/freq_aodv.csv"
        gawk -f ../awk/freq.awk "../trace/lab2/aomdv${cbr_file}.tr" >> "../result/lab2/freq_aomdv.csv"

        echo -e "第 ${i} 组仿真结束\n"
    done

    echo -e "第 ${scenario} 轮仿真结束\n"
done
