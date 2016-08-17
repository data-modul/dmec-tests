#!/bin/bash

echo $1 > "/sys/class/gpio/export"
echo $2 > "/sys/class/gpio/gpio${1}/edge"
