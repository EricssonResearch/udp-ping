set datafile separator ";"

plot 'plot.txt' using 1:($2/1E6) with linespoints title 'Uplink', \
     'plot.txt' using 1:($3/1E6) with linespoints title 'Downlink', \
     'plot.txt' using 1:($4/1E6) with linespoints title 'Roundtrip', \

set xlabel 'Sample'
set ylabel 'Latency [ms]'
set grid

while (1) {
   replot
   pause 0.05
}


