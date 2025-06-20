#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CSV_FILE="$DIR/float_results.csv"
echo "dataset,test,ratio,insertspeed,incrinsertspeed,decspeed" > "$CSV_FILE"

# Benchmarks supporting floating point
declare -a commands=('chimp_benchmarks -f' 'zstd_benchmarks -f' 'lz4_benchmarks -f' 'snappy_benchmarks -f' 'gorilla_benchmarks -f' 'alp_benchmarks -f')

for file in "$DIR"/../datasets/*.csv.gz; do
  fname=$(basename "$file")
  echo "# processing file $fname"
  for t in "${commands[@]}"; do
    echo "#" $t
    output=$(./$t -e .csv.gz "$DIR/../datasets")
    echo "$output"
    lastline=$(echo "$output" | grep -E '^[0-9 .]+' | tail -n 1)
    ratio=$(echo "$lastline" | awk '{print $1}')
    insertspeed=$(echo "$lastline" | awk '{print $2}')
    incrinsertspeed=$(echo "$lastline" | awk '{print $3}')
    decspeed=$(echo "$lastline" | awk '{print $4}')
    echo "$fname,$t,$ratio,$insertspeed,$incrinsertspeed,$decspeed" >> "$CSV_FILE"
  done
  echo
  echo
done
