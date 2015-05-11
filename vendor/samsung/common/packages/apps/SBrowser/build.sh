#!/bin/bash
set -e
set -x
export PATH=/opt/tools/jdk1.6.0_45/bin:$PATH:
export USE_CCACHE=1
export CCACHE_EXE=/usr/bin/ccache
./src/build_libsbrowser.sh highend
echo Build success.