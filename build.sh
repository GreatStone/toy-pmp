SOURCE_DIR="$(cd $(dirname "$0")/; pwd)"

function build_libunwind() {
    local libunwind_src_path=${SOURCE_DIR}/third-party/libunwind
    if [ ! -d ${libunwind_src_path} ]; then
        echo "libunwind not found."
        return -1
    fi
    cd ${libunwind_src_path}

    ./autogen.sh
    if [ $? -ne 0 ]; then
        echo "libnunwind autogen failed"
        return -1
    fi

    ./configure
    if [ $? -ne 0 ]; then
        echo "libunwind configure failed"
        return -1
    fi

    make install prefix=${SOURCE_DIR}/deps/
    return $?
}

function build_gflags() {
    local gflags_src_path=${SOURCE_DIR}/third-party/gflags
    if [ ! -d ${gflags_src_path} ]; then
        echo "gflags not found."
        return -1
    fi
    cd ${gflags_src_path}

    mkdir build && cd build
    cmake -DCMAKE_INSTALL_PREFIX=${SOURCE_DIR}/deps ..
    if [ $? -ne 0 ]; then
        echo "gflags cmake failed"
        return -1
    fi

    make install
    if [ $? -ne 0 ]; then
        echo "gflags install failed"
        return -1
    fi

    return 0
}

function clear_deps() {
    rm -rf deps/*
    cd ${SOURCE_DIR}/third-party/libunwind && make clean
}

#clear_deps
#build_libunwind
#build_gflags
clang++ -std=c++11 -g -fomit-frame-pointer -I ${SOURCE_DIR}/deps/include -lpthread -llzma ${SOURCE_DIR}/profiler.cpp ${SOURCE_DIR}/deps/lib/libgflags.a ${SOURCE_DIR}/deps/lib/libunwind-ptrace.a ${SOURCE_DIR}/deps/lib/libunwind-generic.a ${SOURCE_DIR}/deps/lib/libunwind.a -o ${SOURCE_DIR}/pmp
