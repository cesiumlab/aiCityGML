@echo off
chcp 65001 >nul
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

mkdir build 2>nul
cd build

cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja
