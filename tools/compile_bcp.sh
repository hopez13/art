#!/system/bin/sh
#
# Copyright (C) 2020 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Script to recompile the non-APEX jars in the boot class path and
# system server jars for on-device signing following an ART Module
# update.
#
# For testing purposes, prepare Android device using:
#
#   $ adb root
#   $ adb shell setenforce 0
#
# TODO: this is script is currently for proof-of-concept. It does not
# respect many of the available dalvik.vm properties, e.g. affinity,
# threads, etc nor dalvik.vm.dex2oat-resolve-startup-strings.
#
# TODO: some of the system server jars seem to be installed on device
# for 32-bit and 64-bit, but services.jar artifacts are just one arch.
# Not sure why both flavors are present. Does this script need to generate
# both?
#
# TODO: Logging failure / error handling.
#
# TODO: Purge dalvik-cache on APEX update?

# Output directory for generated files.
output=/data/misc/apexdata/com.android.art/system/framework

# Determine if ART APEX is in the factory image.
function is_factory() {
  local current_apex_info
  current_apex_info=$(grep 'com.android.art.*isActive="true"' /apex/apex-info-list.xml)
  [[ "${current_apex_info}" = *"isFactory=\"true\""* ]]
}

function mkdir_clean() {
  # mkdir_clean <dir_path>
  local dir_path=$1
  rm -rf "${dir_path}"
  mkdir -p "${dir_path}"
}

function compilable_boot_extension_jar() {
  [[ "$1" != *"/apex/com.android.art/"* ]]
}

function compilable_system_server_jar() {
  [[ "$1" != "/apex"* ]]
}

function check_boot_extensions() {
  # Placeholder to check the integrity of boot extensions.
  # Returns 0 on success, 1 otherwise.
  local jar
  for jar in ${DEX2OATBOOTCLASSPATH//:/ }; do
    if ! compilable_boot_extension_jar ${jar} ; then
      continue
    fi

    local arch
    for arch in ${archs}; do
      local stem=boot-$(basename $jar .jar)
      for extension in "art" "oat" "vdex" ; do
        local artifact=${output}/${arch}/${stem}.${extension}
        echo -n "Checking ${artifact} ... "
        if [ ! -f ${artifact} ] ; then
          echo "FAIL"
          return 1
        fi
        echo "OK"
      done
    done
  done
  return 0
}

function check_system_server() {
  # Placeholder to check the integrity of boot extensions.
  # Returns 0 on success, 1 otherwise.

  local jar
  for jar in ${SYSTEMSERVERCLASSPATH//:/ }; do
    if ! compilable_system_server_jar ${jar}; then
      continue
    fi

    local stem=$(basename $jar .jar)

    for extension in "art" "odex" "vdex"; do
      local artifact=${output}/oat/${systemserver_arch}/${stem}.${extension}
      echo -n "Checking ${artifact} ... "
      if [ ! -f ${artifact} ] ; then
        echo "FAIL"
        return 1
      fi
      echo OK
    done

    local profile_file=/system/framework/${stem}.jar.prof
    if [ -f "${profile_file}" ] ; then
      local profile_arg=--profile-file=${profile_file}
      local filter=speed-profile
    else
      local profile_arg=""
      local filter=speed
    fi

    $dexoptanalyzer --dex-file=${jar} \
                    --compiler-filter=${filter} \
                    --image=${output}/framework/boot.art \
                    --isa=${systemserver_arch} \
                    --oat-file=${output}/oat/${systemserver_arch}/${stem}.odex \
                    --vdex-file=${output}/oat/${systemserver_arch}/${stem}.vdex \
                    --zip-file=${jar}
    echo Status $?
  done

  return 0
}

function compile_boot_extensions() {
  # Determine which boot class path jars to compile.
  local device_bcp_list=""
  local device_bcp_dex_files=""
  local jar
  for jar in ${DEX2OATBOOTCLASSPATH//:/ }; do
    if ! compilable_boot_extension_jar ${jar}; then
      continue
    fi
    device_bcp_list="${device_bcp_list}${device_bcp_list:+:}${jar}"
    device_bcp_dex_files="$device_bcp_dex_files --dex-file=${jar}"
  done

  # Compile the boot class path elements that are present on device.
  local arch
  for arch in ${archs}; do
    local arch_output="${output}/$arch"
    mkdir_clean "${arch_output}"

    echo -n "Compiling ${device_bcp_list} ($arch) ... "
    ${dex2oat} --avoid-storing-invocation \
      --compiler-filter=speed-profile \
      --profile-file=/system/etc/boot-image.prof \
      --dirty-image-objects=/system/etc/dirty-image-objects \
      --runtime-arg -Xbootclasspath:${DEX2OATBOOTCLASSPATH} \
      --boot-image=/apex/com.android.art/javalib/boot.art \
      ${device_bcp_dex_files} \
      --generate-debug-info \
      --image-format=lz4hc \
      --strip \
      --oat-file=${arch_output}/boot.oat \
      --image=${arch_output}/boot.art \
      --android-root=out/empty \
      --abort-on-hard-verifier-error \
      --instruction-set=$arch \
      --generate-mini-debug-info
    # TODO: Check error - no error / ENOSPC / other
    status=$?
    if [ ! $status ]; then
      echo "FAIL (${status})"
      return 1
    fi
    echo "OK"
  done
}

function compile_system_server() {
  # Compile system_server and related jars.
  local arch_output="${output}/oat/${systemserver_arch}"
  mkdir_clean "${arch_output}"

  local classloader_context=""
  local jar
  for jar in ${SYSTEMSERVERCLASSPATH//:/ }; do
    # Skip class path components in APEXes
    if [[ ${jar} = "/apex"* ]]; then
      continue
    fi

    # Add profile if it exists. Only services.jar has a profile in AOSP.
    local stem=$(basename $jar .jar)
    local profile_file=/system/framework/${stem}.jar.prof
    if [ -f "${profile_file}" ] ; then
      local profile_arg=--profile-file=${profile_file}
      local filter=speed-profile
    else
      local profile_arg=""
      local filter=speed
    fi

    # Add updatable boot class path packages file if there is a property for it.
    local updatable_bcp_file=$(getprop dalvik.vm.dex2oat-updatable-bcp-packages-file)
    if [ "${updatable_bcp_file}" -a -f "${updatable_bcp_file}" ] ; then
      local updatable_bcp_file_arg="--updatable-bcp-packages-file=${updatable_bcp_file}"
    fi

    echo -n "Compiling ${jar} (${systemserver_arch} ${filter} PCL[${classloader_context}]) ... "
    $dex2oat --avoid-storing-invocation \
            --runtime-arg -Xbootclasspath:${DEX2OATBOOTCLASSPATH} \
            --class-loader-context=PCL[${classloader_context}] \
            --boot-image=/apex/com.android.art/javalib/boot.art:${output}/boot-framework.art \
            --dex-file=${jar} \
            --oat-file=${arch_output}/${stem}.odex \
            --app-image-file=${arch_output}/${stem}.art \
            --android-root=out/empty \
            --instruction-set=${systemserver_arch} \
            --abort-on-hard-verifier-error \
            --compiler-filter=$filter \
            --generate-mini-debug-info \
            --compilation-reason=prebuilt \
            --image-format=lz4 \
            --resolve-startup-const-strings=true \
            ${profile_arg} \
            ${updatable_bcp_file_arg}
    # TODO: Check error - no error / ENOSPC / other
    status=$?
    if [ ! ${status} ]; then
      echo "FAIL (${status})"
      return 1
    fi
    echo "OK"

    classloader_context=${classloader_context}${classloader_context:+:}${jar}
  done
}

# Determine candidate architectures for this device.
case `getprop ro.product.cpu.abi` in
  arm*)
    arch32=arm
    arch64=arm64
    ;;
  x86*)
    arch32=x86
    arch64=x86_64
    ;;
  *)
    echo "Unknown abi"
    exit -1
esac

# Determine architectures to use for Zygote. system_server runs under the primary
# architecture. Prefer dex2oat64 if device supports 64-bits.
case $(getprop ro.zygote) in
  zygote32)
    # The primary architecture is 32-bits.
    archs="$arch32"
    systemserver_arch="$arch32"
    dex2oat=/apex/com.android.art/bin/dex2oat32
    ;;
  zygote32_64)
    # The primary architecture is 32-bits and the secondary is 64-bits.
    archs="$arch32 $arch64"
    systemserver_arch="$arch32"
    dex2oat=/apex/com.android.art/bin/dex2oat64
    ;;
  zygote64_32)
    # The primary architecture is 64-bits and the secondary is 32-bits.
    archs="$arch32 $arch64"
    systemserver_arch="$arch64"
    dex2oat=/apex/com.android.art/bin/dex2oat64
    ;;
  zygote64)
    # Primary architecture is 64-bits.
    archs="$arch64"
    systemserver_arch="$arch64"
    dex2oat=/apex/com.android.art/bin/dex2oat64
    ;;
  *)
    echo "Unknown ro.zygote value"
    exit -1
    ;;
esac

dexoptanalyzer=/apex/com.android.art/bin/dexoptanalyzer

case "$1" in
  --check)
    if is_factory() ; then
      rm -rf ${output} # There should be no output directory
      exit 0
    fi
    ;& # Fall through
  --force-check)
    check_boot_extensions && check_system_server || exit 1
    exit 0
    ;;
  --clean)
    rm -rf ${output}
    ;;
  --compile)
    if is_factory() ; then
      rm -rf ${output} # There should be no output directory
      exit 0
    fi
    ;& # Fall through
  --force-compile)
    rm -rf ${output}
    compile_boot_extensions && compile_system_server || exit 1
    ;;
    *)
    echo "No matching command"
    exit 64 # Usage in sysexits.h
    ;;
esac
