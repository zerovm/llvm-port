#!/bin/sh

BUILD_DIR="build_debug"

if [ ! -d $BUILD_DIR ]; then
  mkdir $BUILD_DIR
fi

cd $BUILD_DIR

../configure --host=x86_64-nacl --target=x86_64-nacl \
--enable-static --disable-shared \
--enable-optimized=no --enable-pic=yes \
--disable-pthreads --disable-threads \
--enable-jit=yes --enable-targets=x86,x86_64,cpp \
--enable-assertions --with-gcc-toolchain=${ZVM_PREFIX}/x86_64-nacl \
--prefix=/ --exec-prefix=/

CPPFLAGS='-O0' make -j8
