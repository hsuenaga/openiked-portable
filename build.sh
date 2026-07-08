#!/usr/bin/env sh
export DESTDIR="/Users/hsuenaga/projects/github.com/build_ios_env/dest/macosx"
CURDIR=`pwd`

cat << EOS > compile_flags.txt
-I${DESTDIR}/usr/include
-I${CURDIR}/iked
-I${CURDIR}/compat
EOS

if [ "x$1" = "x-c" ]; then
	if [ -d "build" ]; then
		if [ -d "build.prev" ]; then
			rm -rf "build.prev"
		fi
		mv build build.prev
	fi
fi

cmake -B build \
	-G Xcode \
	-DBUILD_LIBRARY=ON \
	-DTHREAD=ON \
	-DCMAKE_INSTALL_RPATH=@loader_path/../lib \
	-DCMAKE_TOOLCHAIN_FILE="./ios.toolchain.cmake" \
	-DPLATFORM=MAC_ARM64 \
	-DDEPLOYMENT_TARGET=26.0 &&
cmake --build build

APP="./build/swift/Debug/openiked.app/Contents/MacOS/openiked" 
if [ -x $APP ]; then
	$APP
fi
