#!/usr/bin/env bash
# Ollie 本地逻辑单测（PC 上跑，不依赖硬件）
# 默认用 zig（安装：py -m pip install ziglang）；换编译器：CXX="g++" bash test/run_tests.sh
set -euo pipefail
cd "$(dirname "$0")/.."

CXX=${CXX:-"py -m ziglang c++ -target x86_64-windows-gnu -w"}
mkdir -p build

$CXX -std=c++17 -Itest/mocks -I. \
  test/test_main.cpp test/Face_stub.cpp \
  Stats.cpp Buttons.cpp Buzzer.cpp LightSensor.cpp MoodLight.cpp PetState.cpp ChatLogic.cpp Dialog.cpp \
  -o build/olllie_tests.exe

./build/olllie_tests.exe
