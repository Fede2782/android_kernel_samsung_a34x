#!/bin/bash

mkdir bin
export PATH="$(pwd)/bin:$PATH"

sudo apt-get install curl wget -y

APK_URL="$(curl -s "https://api.github.com/repos/topjohnwu/Magisk/releases/latest" | grep -oE 'https://[^\"]+\.apk')"
wget -O "magisk.zip" "$APK_URL"
unzip "magisk.zip" "lib/x86_64/libmagiskboot.so"
cp "lib/x86_64/libmagiskboot.so" "bin/magiskboot"
chmod +x "bin/magiskboot"

curl https://storage.googleapis.com/git-repo-downloads/repo > bin/repo
chmod a+x bin/repo

mkdir aosp-kernel && cd aosp-kernel
repo init -u https://android.googlesource.com/kernel/manifest -b common-android15-6.6 --depth=1
repo sync -j$(nproc --all)
ln -s "$(pwd)/prebuilts" "$(pwd)/../kernel/prebuilts"
cd ..

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
export SEC_BUILDNUMBER="ogkiA346BXXUBEYI7"

chmod +x ./kernel_device_modules-6.6/build.sh
./kernel_device_modules-6.6/build.sh

cd ..
wget -O boot.img https://github.com/Fede2782/proprietary_vendor_samsung_a34x/releases/latest/download/boot.img
mkdir bootimg && cd bootimg
magiskboot unpack ../boot.img
cp ../out/target/product/a34x/obj/KLEAF_OBJ/dist/kernel_device_modules-6.6/mgk_64_k66_kernel_aarch64.user/Image kernel
PATCHVBMETAFLAG=true magiskboot repack ../boot.img out-boot.img
mv out-boot.img ../boot.img
cd ..
