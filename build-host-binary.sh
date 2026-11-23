cd host
cmake -S . -B build
cmake --build build -j
./build/simppeliTCU_host --prefs ../simppeliTCU_host_prefs.txt --replay sample-can.log --loop --port 8090