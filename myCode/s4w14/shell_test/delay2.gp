set terminal pngcairo enhanced font "Arial,16"
set output "delay3.png"
# set title "平均延迟 vs 发包速率"
set xlabel "节点发包速率 (Packet/s)"
set ylabel "网络平均端到端延时 (s)"
set xtics 5
set xrange [0:30]
set ytics 0.5
set yrange [0:3]
set grid

set multiplot

# 绘制AODV的点线图
datafile_aodv = "delay_aodv.csv"
stats datafile_aodv nooutput
n_aodv = STATS_records
x_aodv(i) = 3.0 + (i-1) * 3.0
plot datafile_aodv using (x_aodv($0+1)):1 with linespoints title "AODV" linestyle 2 lc rgb "red"

# 绘制AOMDV的点线图
datafile_aomdv = "delay_aomdv.csv"
stats datafile_aomdv nooutput
n_aomdv = STATS_records
x_aomdv(i) = 3.0 + (i-1) * 3.0
replot datafile_aomdv using (x_aomdv($0+1)):1 with linespoints title "AOMDV" linestyle 7 lc rgb "green"