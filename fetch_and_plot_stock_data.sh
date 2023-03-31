#!/bin/sh

symbol=

### Check program options.
while [ X"$1" != X-- ]
do
    case "$1" in
       -s) symbol="$2"
           shift 2
           ;;
   -start) start_date="$2"
           shift 2
           ;;
   -debug) echo "DEBUG ON"
           set -x
           DEBUG="yes"
           trap '' SIGHUP SIGINT SIGQUIT SIGTERM
           shift 1
           ;;
       -*) echo "${program}: Invalid parameter $1: ignored." 1>&2
           shift
           ;;
        *) set -- -- $@
           ;;
    esac
done
shift           # remove -- of arguments

if [ ! -z $symbol ]; then
	symbol=$(echo "$symbol" | tr "[a-z]" "[A-Z]")
fi

if [ -z "${start_date}" ] ; then
   # Get today's date in the format of YYYY-MM-DD
   start_date=$(date +"%Y-%m-%d")

   # Check if today is a Saturday (weekday number 6) or Sunday (weekday number 0)
   weekday=$(date +"%u")
   if [ $weekday -eq "6" ] ; then
       start_date=$(date -d "$start_date -1 day" +"%Y-%m-%d")  # Set start_date to Friday
   elif [ $weekday -eq "7" ] ; then
       start_date=$(date -d "$start_date -2 day" +"%Y-%m-%d")  # Set start_date to Friday
   fi
   # echo $start_date  # Output e.g. '2023-03-01' if today is a weekday, or '2023-02-28' if today is Sunday
fi

./alpaca_memory_price_fetcher -symbol "${symbol}" -start "${start_date}" | \
awk -F"," '{if(NF==8){print $5}}' | sed 's/Close=//g' | awk '{print NR, $1}' > stuff.bin 

last_y_value=$(tail -1 stuff.bin | awk '{print $2}') 
gnuplot -e "set datafile separator ' '; set grid xtics ytics linetype 1 dashtype 2; 
set xlabel 'Trading Minutes'; set ylabel 'Price ($)'; 
set title sprintf('"${symbol}" Closing Prices (Current Price = $%.3f)', $last_y_value); plot 'stuff.bin' using 1:2 with lines linecolor rgb 'black' title 'Data Plot'; pause -1"

rm -f stuff.bin
