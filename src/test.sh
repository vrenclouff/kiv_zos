#!/bin/bash

echo "************ 1 thread ***********"
./pseudo_fat output.fat 1
sleep 1
echo "************ 2 thread ***********"
./pseudo_fat output.fat 2
sleep 1
echo "************ 3 thread ***********"
./pseudo_fat output.fat 3
sleep 1
echo "************ 4 thread ***********"
./pseudo_fat output.fat 4
