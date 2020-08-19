#!/usr/bin/env python3
#
# [VPYTHON:BEGIN]
# python_version: "3.8"
# [VPYTHON:END]
#
# Copyright 2020, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
from collections import namedtuple

SingleRunConfiguration = namedtuple("SingleRunConfiguration", [
                                    'android_root',
                                    'android_art_root',
                                    'android_i18l_root',
                                    'android_tzdata_root',
                                    'boot_image',
                                    'chroot',
                                    'compile_flags',
                                    'dalvikvm',
                                    'debugger',
                                    'agents',
                                    'dev_mode',
                                    'dex2oat_binary',
                                    'experiments',
                                    'javac_flags',
                                    'android_flags',
                                    'gdb_bin',
                                    'gdb_flags',
                                    'have_image',
                                    'target',
                                    'linux_bionic',
                                    ])


def main():
    parser = argparse.ArgumentParser("run-test-jar")
    parser.add_argument("main", action="store", default="Main")
    parser.add_argument("--quiet", action="store_true",
                        default=False, help="quiet mode")
    parser.add_argument("--dev", action="store_true",
                        default=False, help="dev mode")
    parser.add_argument("--dex2oat-rt-timeout", action="store",
                        type=int, default=360, help="timeout in seconds")
    parser.add_argument("--jvmti", action="store_true",
                        default=False, help="enable jvmti")
    parser.add_argument("--add-libdir-argument",
                        action="store_true", default=False, help="libdir args")
    parser.add_argument("-O", "--ndebug", action="store_true",
                        default=False, help="run ndebug")
    parser.add_argument("--lib", action="store", required=True, help="lib")
    parser.add_argument("--gc-stress", action="store_true",
                        default=False, help="gc-stress")
    parser.add_argument("--args", "--testlib",
                        action="append", default=[], dest="args")
    parser.add_argument("--compiler-only-option", action="append", default=[])
    parser.add_argument("-Xcompiler-option", action="append",
                        default=[], dest="compiler_option")
    parser.add_argument("--create-runner", action="store_true", default=False)
    parser.add_argument("--runtime-option", action="append", default=[])
    parser.add_argument("--boot", action="store")
    parser.add_argument("--relocate", action="store_true", default=False)
    parser.add_argument("--no-relocate", action="store_false", dest="relocate")
    parser.add_argument("--prebuild", action="store_true", default=False)
    parser.add_argument("--no-prebuild", action="store_false", dest="prebuild")
    parser.add_argument("--compact-dex-level", action="store", type=int)
    parser.add_argument("--jvmti-redefine-stress",
                        action="store_true", default=False)
    parser.add_argument("--jvmti-step-stress",
                        action="store_true", default=False)
    parser.add_argument("--jvmti-field-stress",
                        action="store_true", default=False)
    parser.add_argument("--jvmti-trace-stress",
                        action="store_true", default=False)
    parser.add_argument("--no-app-image", action="store_false",
                        default=True, dest="app_image")
    parser.add_argument("--no-secondary-app-image", action="store_false",
                        default=True, dest="secondary_app_image")
    parser.add_argument("--secondary-class-loader-context", action="store")
    parser.add_argument("--no-secondary-compilation", action="store_false",
                        default=True, dest="secondary_compilation")
    parser.add_argument("--bionic", action="store_true",
                        default=False, dest="linux_bionic")
    parser.add_argument("--no-image", action="store_false",
                        default=True, dest="have_image")
    parser.add_argument("--secondary", action="store")
    parser.add_argument("--with-agent", action="append", default=[])
    parser.add_argument("--debug-wrap-agent",
                        action="store_true", default=False)
    parser.add_argument("--debug-agent", action="store_true", default=False)
    parser.add_argument("--debug", action="store_true", default=False)
    parser.add_argument("--gdbserver-port", action="store",
                        type=int, default=5039)
    parser.add_argument("--gdbserver-bin", action="store", default="gdbserver")
    parser.add_argument("--gdbserver", action="store_true", default=False)
    parser.add_argument("--gdb", action="store_true", default=False)
    parser.add_argument("--gdb-arg", action="append", default=[])
    parser.add_argument("--zygote", action="store_true", default=False)
    parser.add_argument("--invoke-with", action="append", default=[])
    parser.add_argument("--no-optimize", action="store_false",
                        default=True, dest="optimize")
    parser.add_argument("--chroot", action="store")
    parser.add_argument("--android-root", action="store")
    parser.add_argument("--android-i18l-root", action="store")
    parser.add_argument("--android-art-root", action="store")
    parser.add_argument("--android-tzdata-root", action="store")
    parser.add_argument("--instruction-set-features",
                        action="store", default="")
    parser.add_argument("--timeout", action="store", type=int)
    parser.add_argument("--experimental", action="append", default=[])
    parser.add_argument("--external-log-tags",
                        action="store_true", default=False)
    parser.add_argument("--dry-run", action="store_true", default=False)
    parser.add_argument("--vdex", action="store_true", default=False)
    parser.add_argument("--dm", action="store_true", default=False)
    parser.add_argument("--vdex-filter", action="store")
    parser.add_argument("--vdex-arg", action="append", default=[])
    parser.add_argument("--sync", action="store_true", default=False)
    parser.add_argument("--profile", action="store_true", default=False)
    parser.add_argument("--random-profile", action="store_true", default=False)

    target_group = parser.add_mutually_exclusive_group()
    target_group.add_argument(
        "--host", action="store_const", const="host", default="target", dest="target")
    target_group.add_argument(
        "--jvm", action="store_const", const="jvm", default="target", dest="target")
    target_group.add_argument(
        "--target", action="store_const", const="target", default="target", dest="target")

    zipapex_group = parser.add_mutually_exclusive_group()
    zipapex_group.add_argument("--runtime-extracted-zipapex", action="store")
    zipapex_group.add_argument("--runtime-zipapex", action="store")

    runmode_group = parser.add_mutually_exclusive_group()
    runmode_group.add_argument("--optimizing", action="store_const",
                               const="optimizing", dest="run_mode", default="optimizing")
    runmode_group.add_argument(
        "--interpreter", action="store_const", const="interpreter", dest="run_mode")
    runmode_group.add_argument(
        "--jit", action="store_const", const="jit", dest="run_mode")
    runmode_group.add_argument(
        "--baseline", action="store_const", const="baseline", dest="run_mode")

    verify_group = parser.add_mutually_exclusive_group()
    verify_group.add_argument("--no-verify", action="store_const",
                              const="no-verify", default="verify", dest="verify")
    verify_group.add_argument(
        "--verify", action="store_const", const="verify", default="verify", dest="verify")
    verify_group.add_argument("--verify-soft-fail", action="store_const",
                              const="soft", default="verify", dest="verify")

    address_group = parser.add_mutually_exclusive_group()
    address_group.add_argument(
        "--32", action="store_const", const=32, dest="address_size", default=32)
    address_group.add_argument(
        "--64", action="store_const", const=64, dest="address_size")

    base_args = parser.parse_args()


if __name__ == "__main__":
    main()
