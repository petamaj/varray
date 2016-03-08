#!/bin/sh

g++ -g3 -O3 --std c++11 ir.cpp -o ir
g++ -g3 -O3 --std c++11 ir-ll.cpp -o ir-ll

sync
wait

./ir
wait

./ir-ll list
wait

./ir-ll deque
