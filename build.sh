#!/usr/bin/env sh
export DESTDIR="/Users/hsuenaga/projects/github.com/build_ios_env/dest/macosx"
CURDIR=`pwd`
APP_XCODE="./build/swift/Debug/openiked.app/Contents/MacOS/openiked" 
APP_NINJA="./build/swift/openiked.app/Contents/MacOS/openiked"

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
elif [ "x$1" = "x-e" ]; then
	if [ -x $APP ]; then
		lldb $APP
	fi
	exit 0
fi

CM_ARGS_PLAIN=(
	-GXcode
	-DCMAKE_MACOSX_BUNDLE=YES
	-DCMAKE_SYSTEM_NAME=iOS
	-DCMAKE_OSX_SYSROOT=iphoneos
	-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=BSVYLYB6S2
	-DCMAKE_OSX_DEPLOYMENT_TARGET=26.0
)

CM_ARGS=(
#	-G Xcode
	-G Ninja
	-DHOMEBREW=ON
	-DBUILD_LIBRARY=ON
	-DTHREAD=ON
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	-DCMAKE_INSTALL_RPATH=@loader_path/../lib
	-DCMAKE_TOOLCHAIN_FILE="./ios.toolchain.cmake"
	-DPLATFORM=MAC_ARM64
	-DDEPLOYMENT_TARGET=26.0
	-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=BSVYLYB6S2
)
CMD="cmake -B build ${CM_ARGS[@]}"
echo $CMD
eval $CMD &&
cmake --build build
ret=$?

if [ $ret -ne 0 ]; then
	echo "BUILD FAILURE."
	exit $ret
fi

if [ -x $APP_XCODE ]; then
	$APP_XCODE
elif [ -x $APP_NINJA ]; then
	$APP_NINJA
else
	echo "NO EXECUTABLE FOUND."
	exit 1
fi
