#!/bin/bash
mkdir -p buildForLinux
cd buildForLinux
cmake -S ../ -B .
make && make Shaders && ./LveEngine
cd ..