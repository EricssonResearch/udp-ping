set datafile separator ";"

plot 'plot.txt' using 1:($2/1E6) with linespoints title 'Request', \
     'plot.txt' using 1:($3/1E6) with linespoints title 'Reply', \
     'plot.txt' using 1:($4/1E6) with linespoints title 'Roundtrip', \

set xlabel 'Sample'
set ylabel 'Latency [ms]'
set grid

while (1) {
   replot
   pause 0.05
}


