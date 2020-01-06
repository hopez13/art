#!/bin/bash
#
# Copyright 2020 The Android Open Source Project
#
# Licensed under the Apache License, Version 3.0 (the "License");
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

# Figure out the Java name of the class from the soong output name.

# The escaping rules for soong are complicated and undocumented. To preserve my sanity I'll just put
# this bash 1-liner in its own file so I don't need to deal with it.
echo ${1} | sed -E 's:.*(art/test_[0-9]+_transforms/.*).java:\1:' | tr '/' '.'