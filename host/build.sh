cmake -S . -B build
cmake --build build -j
./build/simppeliTCU_host --replay sample-can.log --loop --port 8090