#!/bin/bash

set -e
set -x

cd src

BUILD_TYPE=$1
if [ -z "$BUILD_TYPE" ]
then
    echo Invalid BUILD_TYPE >&2
    exit 1
fi
shift

. build/android/envsetup.sh
export PATH=$PATH:../depot_tools
export PATH=$PATH:/usr/lib/jvm/jdk1.6/bin

android_gyp -Denable_s_android_browser=1 -Denable_s=1 --generator-output=out/$BUILD_TYPE $*
ninja -C out/$BUILD_TYPE/out/Release libsbrowser
