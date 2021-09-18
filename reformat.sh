#!/usr/bin/env bash
set -x

clang-format -i \
    qmk/common/*.{c,h} \
    qmk/keyboards/gmmk/pro/accurate0/*.{c,h,inc} \
    qmk/override/*.{c,h} \
    hidd/hid.cpp
