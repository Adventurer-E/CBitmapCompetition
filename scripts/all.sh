#!/bin/bash
######################
# To add a technique, simply append the file name of your executable to the commands array below
#######################
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CSV_FILE="$DIR/results.csv"
echo "dataset,test,ratio,speed,decspeed" > "$CSV_FILE"
declare -a commands=('slow_roaring_benchmarks -r' 'malloced_roaring_benchmarks -r' 'roaring_benchmarks -r' 'roaring_benchmarks -c -r' 'roaring_benchmarks' 'roaring_benchmarks -c'   'ewah32_benchmarks'  'ewah64_benchmarks' 'wah32_benchmarks' 'chimp_benchmarks' 'concise_benchmarks' );
echo "# For each data set we report the compression ratio (percentage of the uncompressed size), the compression speed (cycles per byte) and the decompression speed (cycles per byte)"
for f in  census-income census-income_srt census1881  census1881_srt  weather_sept_85  weather_sept_85_srt wikileaks-noquotes  wikileaks-noquotes_srt ; do
  echo "# processing file " $f
  for t in "${commands[@]}"; do
     echo "#" $t
    output=$(./$t CRoaring/benchmarks/realdata/$f)
    echo "$output"
    lastline=$(echo "$output" | grep -E '^[0-9 .]+$' | tail -n 1)
    ratio=$(echo "$lastline" | awk '{print $1}')
    speed=$(echo "$lastline" | awk '{print $2}')
    decspeed=$(echo "$lastline" | awk '{print $3}')
    echo "$f,$t,$ratio,$speed,$decspeed" >> "$CSV_FILE"
  done
  echo
  echo
done
