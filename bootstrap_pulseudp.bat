@echo off
setlocal

mkdir build
cd build

cmake .. -DCMAKE_INSTALL_PREFIX=..\dist

cmake --build . --config Release
cmake --install . --config Release

endlocal