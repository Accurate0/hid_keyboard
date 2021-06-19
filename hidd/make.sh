#!/usr/bin/env bash
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    make -f Makefile.linux $@
else
    make -f Makefile.mingw $@
fi
