#
# Copyright (C) 2022 The Android Open Source Project
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


def build(ctx):
  from os.path import relpath
  from time import sleep
  ctx.bash("echo foo > foo.tmp")
  ctx.bash("ls -l --full-time *.tmp")
  ctx.bash("sha1sum *.tmp")
  ctx.rbe_wrap(["--output_files", relpath(ctx.test_dir / "bar.tmp", ctx.rbe_exec_root),
                "cp", ctx.test_dir / "foo.tmp", ctx.test_dir / "bar.tmp"])
  ctx.bash("ls -l --full-time *.tmp")
  ctx.bash("sha1sum *.tmp")
  print(r"$ echo bar!!! > bar.tmp", flush=True)
  ctx.bash("echo bar!!! > bar.tmp")
  ctx.bash("ls -l --full-time *.tmp")
  ctx.bash("sha1sum *.tmp")
  ctx.rbe_wrap(["--output_files", relpath(ctx.test_dir / "baz.tmp", ctx.rbe_exec_root),
                "cp", ctx.test_dir / "bar.tmp", ctx.test_dir / "baz.tmp"])
  ctx.bash("ls -l --full-time *.tmp")
  ctx.bash("sha1sum *.tmp")
  return

  ctx.bash("./generate-sources")
  ctx.default_build(use_desugar=False,
                    api_level=28,
                    javac_classpath=[ctx.test_dir / "transformer.jar"])
