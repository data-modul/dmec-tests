#!/bin/bash

cnt=$1
echo $cnt
for ((i=$((cnt)); i < $((cnt+8)); i++)); do
	if [[ -e "/sys/class/gpio/gpio$i" ]]; then
		echo "Unexporting gpio $i"
		echo $i > /sys/class/gpio/unexport
	else
		echo "Exporting gpio $i"
		echo $i > /sys/class/gpio/export
		echo "Set as $2"
		echo $2 > /sys/class/gpio/gpio$i/direction
	fi
done

cd /sys/class/gpio
echo $PWD
echo in > gpio$((cnt+6))/direction
echo "Input: $((cnt+6))"
echo in > gpio$((cnt+7))/direction
echo "Input: $((cnt+7))"
echo 1 > gpio$((cnt+4))/value
cat gpio$((cnt+6))/value
cat gpio$((cnt+7))/value

