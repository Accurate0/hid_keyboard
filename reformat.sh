#!/usr/bin/env bash
set -x

clang-format -i \
    qmk/common/*.c \
    qmk/keyboards/gmmk/pro/accurate0/*.c \
    qmk/keyboards/gmmk/pro/accurate0/*.inc \
    qmk/override/*.c \
    hidd/hid.cpp
