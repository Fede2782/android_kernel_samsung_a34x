#!/bin/bash

cd kernel

FTP="
build/kernel/_setup_env.sh
build/kernel/kleaf/impl/stamp.bzl
build/kernel/kleaf/impl/kernel_env.bzl
"

for f in $FTP; do
  sed -i "s/SOURCE_DATE_EPOCH=0/SOURCE_DATE_EPOCH\=\\\"\$\(date \+\%s\)\\\"/g" "$f"
done

sed -i "s/-maybe-dirty//g" "build/kernel/kleaf/impl/stamp.bzl"
sed -i "s/stable_scmversion_cmd = _get_status_at_path.*/stable_scmversion_cmd = \"echo \'\'\"/g" "build/kernel/kleaf/impl/stamp.bzl"
sed -i 's|SOURCE_DATE_EPOCH=0|SOURCE_DATE_EPOCH=\\"$(date +%s)\\"|' "kernel_device_modules-6.6/scripts/gen_build_config.py"

python kernel_device_modules-6.6/scripts/gen_build_config.py --kernel-defconfig mediatek-bazel_defconfig --kernel-defconfig-overlays "sec_ogki_fragment.config mt6877_overlay.config mt6877_teegris_5_overlay.config" --kernel-build-config-overlays "" -m user -o ../out/target/product/a34x/obj/KERNEL_OBJ/build.config

export DEVICE_MODULES_DIR="kernel_device_modules-6.6"
export BUILD_CONFIG="../out/target/product/a34x/obj/KERNEL_OBJ/build.config"
export OUT_DIR="../out/target/product/a34x/obj/KLEAF_OBJ"
export DIST_DIR="../out/target/product/a34x/obj/KLEAF_OBJ/dist"
export DEFCONFIG_OVERLAYS="sec_ogki_fragment.config mt6877_overlay.config mt6877_teegris_5_overlay.config"
export PROJECT="mgk_64_k66"
export MODE="user"
export SOURCE_DATE_EPOCH="$(date +%s)"
export BUILD_NUMBER="ogkiA346BXXUBEYI7"

chmod +x ./kernel_device_modules-6.6/build.sh
./kernel_device_modules-6.6/build.sh
