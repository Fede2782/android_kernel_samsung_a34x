#!/bin/bash

export PATH=$(pwd)/toolchain/clang/host/linux-x86/clang-r450784e/bin:$PATH
export CROSS_COMPILE=$(pwd)/toolchain/clang/host/linux-x86/clang-r450784e/bin/aarch64-linux-gnu-
export CC=$(pwd)/toolchain/clang/host/linux-x86/clang-r450784e/bin/clang
export CLANG_TRIPLE=aarch64-linux-gnu-
export ARCH=arm64
export PLATFORM_VERSION=14
export TARGET_SOC=mt6877
export TARGET_BUILD_VARIANT=user

export KCFLAGS=-w
export CONFIG_SECTION_MISMATCH_WARN_ONLY=y

make -C $(pwd) O=$(pwd)/out KCFLAGS=-w CONFIG_SECTION_MISMATCH_WARN_ONLY=y LLVM=1 LLVM_IAS=1 a34x_defconfig
make -C $(pwd) O=$(pwd)/out KCFLAGS=-w CONFIG_SECTION_MISMATCH_WARN_ONLY=y LLVM=1 LLVM_IAS=1 -j16

cp out/arch/arm64/boot/Image $(pwd)/arch/arm64/boot/Image

