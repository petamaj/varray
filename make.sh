#!/bin/sh

g++ -O3 --std c++11 ir.cpp -o ir
g++ -O3 --std c++11 ir-ll.cpp -o ir-ll

./ir > /dev/null
./ir-ll > /dev/null
