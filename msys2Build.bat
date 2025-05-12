@echo off

if not exist build  mkdir build
cd build
cmake -S ../ -B . -G "MSYS Makefiles" -DCMAKE_MAKE_PROGRAM=C:/msys64/ucrt64
C:/msys64/usr/bin/make.exe && C:/msys64/usr/bin/make.exe Shaders
start LveEngine.exe
cd..