set datafile separator ";"
plot 'plot.txt' using 1:4 with lines
pause 0.2
reread
set autoscale


