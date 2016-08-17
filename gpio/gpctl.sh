#!/bin/bash

cnt=$2
echo $cnt
for ((i=$((cnt)); i < $((cnt+8)); i++)); do
	if [[ "$1" == '-r' ]]; then
		cat /sys/class/gpio/gpio$i/value
	else
		echo 1 > "/sys/class/gpio/gpio$i/value"
	fi
done
