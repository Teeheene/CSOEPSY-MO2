#!/bin/bash
g++ -g -O0 -pthread -std=c++17 -fsanitize=address,undefined -fno-omit-frame-pointer -o a.out src/*.cpp && ./a.out
