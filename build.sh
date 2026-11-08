#!/bin/sh
# Builds everything in src/ into bin/. Needs CeGCC (arm-mingw32ce) on PATH.
set -e
CC="arm-mingw32ce-gcc -O2 -s -Wall -Wno-unused"
mkdir -p bin
$CC -shared -o bin/libgles_cm.dll src/rotwrap/rotwrap4.c src/rotwrap/rotwrap4.def -lcoredll
$CC -o bin/gltest.exe   src/gltest/gltest.c   -lcoredll
$CC -o bin/cuberun.exe  src/cuberun/cuberun.c -lcoredll
$CC -o bin/d3dmrun.exe  src/d3dmrun/d3dmrun.c -lcoredll
$CC -o bin/bench2d.exe  src/bench2d/bench2d.c -lcoredll
$CC -o bin/dlltest.exe  src/dlltest/dlltest.c -lcoredll
echo "built:"; ls -l bin
