SOURCE_DIR="$(cd $(dirname "$0")/; pwd)"
# customize your compiler
export CC=gcc
export CXX=g++

function build_libunwind() {
    local libunwind_src_path=${SOURCE_DIR}/third-party/libunwind
    if [ ! -d ${libunwind_src_path} ]; then
        echo "libunwind not found."
        return 255
    fi
    cd ${libunwind_src_path}

    ./autogen.sh
    if [ $? -ne 0 ]; then
        echo "libnunwind autogen failed"
        return 255
    fi

    ./configure
    if [ $? -ne 0 ]; then
        echo "libunwind configure failed"
        return 255
    fi

    make install prefix=${SOURCE_DIR}/deps/
    return $?
}

function build_gflags() {
    local gflags_src_path=${SOURCE_DIR}/third-party/gflags
    if [ ! -d ${gflags_src_path} ]; then
        echo "gflags not found."
        return 255
    fi
    cd ${gflags_src_path}

    mkdir build; cd build
    cmake -DCMAKE_INSTALL_PREFIX=${SOURCE_DIR}/deps ..
    if [ $? -ne 0 ]; then
        echo "gflags cmake failed"
        return 255
    fi

    make install
    if [ $? -ne 0 ]; then
        echo "gflags install failed"
        return 255
    fi

    return 0
}

function clear_deps() {
    rm -rf deps/*
    cd ${SOURCE_DIR}/third-party/libunwind && make clean
}

echo "start build"
clear_deps
build_libunwind
if [ $? -ne 0 ]; then
    echo "build libunwind failed"
    exit 255
else
    echo "build libunwind succ"
fi
build_gflags
if [ $? -ne 0 ]; then
    echo "build libgflags failed"
    exit 255
else
    echo "build libgflags succ"
fi

${CXX} -std=c++11 -g -fomit-frame-pointer -I ${SOURCE_DIR}/deps/include -lpthread -llzma ${SOURCE_DIR}/profiler.cpp ${SOURCE_DIR}/deps/lib/libgflags.a ${SOURCE_DIR}/deps/lib/libunwind-ptrace.a ${SOURCE_DIR}/deps/lib/libunwind-generic.a ${SOURCE_DIR}/deps/lib/libunwind.a -o ${SOURCE_DIR}/pmp -static-libgcc -static-libstdc++
if [ $? -ne 0 ]; then
    echo "build pmp failed"
    exit 255
else
    echo "build pmp succ"
fi
