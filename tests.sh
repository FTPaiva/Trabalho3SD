#!/bin/bash

g++ -o processo proc.cpp

start_time=$(date +%s)
r=3
k=0
n=128

for ((i = 1; i <= n; i++)); do
    ./processo $r $k<<EOF


EOF
done

wait
end_time=$(date +%s)
total_time=$((end_time - start_time))

echo "Tempo total: ${total_time} segundos"

rm processo