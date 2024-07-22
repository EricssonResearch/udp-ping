set datafile separator ";"
plot 'plot.txt' using 1:4 with lines
while (1) {
   replot
   pause 0.2
}


