#!/bin/sh

g++ -O3 --std c++11 ir.cpp -o ir
g++ -O3 --std c++11 ir-ll.cpp -o ir-ll

echo "in-place"
time ./ir > /dev/null
echo "linked-list"
time ./ir-ll > /dev/null
