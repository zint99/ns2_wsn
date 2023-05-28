set terminal pngcairo enhanced font "Arial,16"
set output "delay.png"
set title "CBR传输延时"
set xlabel "cbr packet ID"
set ylabel "delays(s)"
set xtics 100
set ytics 0.5
set grid

plot "delay.txt" using 1:2 with points title "AODV" lc rgb "red"