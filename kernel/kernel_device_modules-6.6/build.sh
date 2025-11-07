# SPDX-License-Identifier: GPL-2.0
#!/bin/bash

set -e

# DEVICE_MODULES_DIR=$(basename $(dirname $0))
source "${DEVICE_MODULES_DIR}/kernel/kleaf/_setup_env.sh"

# Generate OGKI files
if [ -n "${SEC_OGKI_OUT_CMDS}" ]; then
  echo "========================================================"
  echo " Running 'sec_ogki_out_cmds' command(s):"
  eval ${SEC_OGKI_OUT_CMDS}
fi

SOURCE_DATE_EPOCH=`date "+%s"`
OGKI_SCMVERSION="{\"SCMVERSION\":\"-ab${BUILD_NUMBER}\", \"SOURCE_DATE_EPOCH\":${SOURCE_DATE_EPOCH}}"
echo "${OGKI_SCMVERSION}" > ./kernel-${KERNEL_VERSION_NUM}/workspace_status.json
SOURCE_DATE_EPOCH=0

build_scope=internal
if [ ! -d "vendor/mediatek/tests/kernel" ]
then
  build_scope=customer
fi

if [ -z ${TARGET} ]
then
  TARGET=${build_scope}_modules_install
  KLEAF_BUILD_TARGET=//${DEVICE_MODULES_DIR}:${PROJECT}_${TARGET}.${MODE}
else
  KLEAF_BUILD_TARGET=${TARGET}.${MODE}
fi
KLEAF_DIST_TARGET=//${DEVICE_MODULES_DIR}:${PROJECT}_${build_scope}_dist.${MODE}

KLEAF_OUT=("--output_user_root=${OUT_DIR} --output_base=${OUT_DIR}/bazel/output_user_root/output_base")
KLEAF_ARGS=("${DEBUG_ARGS} ${SANDBOX_ARGS} \
	--config=stamp --repo_manifest=${ROOT_DIR}/${DEVICE_MODULES_DIR}/fake_manifest.xml \
	--experimental_writable_outputs \
	--noenable_bzlmod \
	--//build/bazel_mgk_rules:kernel_version=${KERNEL_VERSION_NUM}")

set -x
(
	tools/bazel ${KLEAF_OUT} build ${KLEAF_ARGS} ${KLEAF_BUILD_TARGET}
	tools/bazel ${KLEAF_OUT} run ${KLEAF_ARGS} \
	--nokmi_symbol_list_violations_check ${KLEAF_DIST_TARGET} -- --dist_dir=${OUT_DIR}/dist
)
set +x

KLEAF_GKI_CHECKER="no"
if [[ ${MODE} == "user" && ${KLEAF_GKI_CHECKER} != "no" ]]
then
  KLEAF_GKI_CHECKER_COMMANDS=("${KLEAF_GKI_CHECKER_COMMANDS} \
	  -m ${OUT_DIR}/dist/${DEVICE_MODULES_DIR}/${PROJECT}_kernel_aarch64.${MODE}/vmlinux")
  set -x
  (
    ${KLEAF_GKI_CHECKER_COMMANDS} -o file
    ${KLEAF_GKI_CHECKER_COMMANDS} -o config
    ${KLEAF_GKI_CHECKER_COMMANDS} -o symbol
  )
  set +x
fi

