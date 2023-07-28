#
# Copyright (C) 2021 The Android Open Source Project
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

def run(ctx, args):
  ctx.default_run(
      args,
      android_log_tags="*:d",
      diff_min_log_tag="d",
      runtime_option=["-Xmetrics-reporting-mods:100",]
  )

  # Check that log messages from the metrics reporting thread appear in stderr.
  ctx.run(fr"grep -o 'Metrics reporting thread started' '{args.stderr_file}' > matching_lines")
  ctx.run(fr"grep -o 'Metrics reporting thread terminating' '{args.stderr_file}' >> matching_lines")
  ctx.run(fr"mv matching_lines '{args.stderr_file}'")
