# SPDX-License-Identifier: GPL-2.0
#!/bin/bash

set -e

DEVICE_MODULES_DIR=$(basename $(dirname $0))
source "${DEVICE_MODULES_DIR}/kernel/kleaf/_setup_env.sh"

build_scope=internal
if [ ! -d "vendor/mediatek/tests/kernel" ]
then
  build_scope=customer
fi

KLEAF_OUT=("--output_user_root=${OUT_DIR} --output_base=${OUT_DIR}/bazel/output_user_root/output_base")
KLEAF_ARGS=("${DEBUG_ARGS} ${SANDBOX_ARGS} --experimental_writable_outputs --noenable_bzlmod")

set -x
(
  tools/bazel ${KLEAF_OUT} run ${KLEAF_ARGS} \
	--//build/bazel_mgk_rules:kernel_version=${KERNEL_VERSION_NUM} \
	//${DEVICE_MODULES_DIR}:${PROJECT}.user_${build_scope}_abi_update_symbol_list
  tools/bazel ${KLEAF_OUT} run ${KLEAF_ARGS} //${KERNEL_VERSION}:kernel_aarch64_abi_update
)

KLEAF_GKI_CHECKER="no"
if [[ ${MODE} == "user" && ${KLEAF_GKI_CHECKER} != "no" ]]
then
  KLEAF_GKI_CHECKER_COMMANDS=("${KLEAF_GKI_CHECKER_COMMANDS} \
	  -m ${OUT_DIR}/bazel/output_user_root/output_base/execroot/__main__/bazel-out/k8-fastbuild*/bin/${DEVICE_MODULES_DIR}/${PROJECT}_kernel_aarch64.${MODE}/vmlinux")
  set -x
  (
    ${KLEAF_GKI_CHECKER_COMMANDS} -o file
    ${KLEAF_GKI_CHECKER_COMMANDS} -o config
    ${KLEAF_GKI_CHECKER_COMMANDS} -o symbol
  )
  set +x
fi

