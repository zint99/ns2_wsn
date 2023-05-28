set terminal pngcairo enhanced font "Arial,16"
set output "delay.png"
set title "平均延迟 vs 发包速率"
set xlabel "节点发包速率 (Packet/s)"
set ylabel "网络平均端到端延时 (s)"
set xtics 5
set xrange [0:30]
set ytics 0.5
set yrange [0:3]
set grid

# 读取数据文件
datafile = "delay.csv"
stats datafile nooutput
n = STATS_records
x(i) =3.0 + (i-1) * 3.0

# 绘制点线图
plot datafile using (x($0+1)):1 with linespoints title "AODV" linestyle 1 lc rgb "red" 