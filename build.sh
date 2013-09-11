#!/bin/sh

BUILD_DIR="build"

if [ ! -d $BUILD_DIR ]; then
  mkdir $BUILD_DIR
fi

cd $BUILD_DIR

../configure --host=x86_64-nacl --target=x86_64-nacl \
--enable-static --disable-shared \
--prefix=$ZVM_PREFIX/x86_64-nacl \
--enable-optimized=yes --enable-pic=yes \
--disable-pthreads --disable-threads \
--enable-jit=yes --enable-targets=x86,x86_64,cpp \
--disable-assertions --with-gcc-toolchain=${ZVM_PREFIX}/x86_64-nacl

CPPFLAGS='-O0' make -j4
