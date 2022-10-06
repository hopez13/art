#!/usr/bin/python3

import os, sys, re

with open(sys.argv[1], "rt") as f:
  lines = f.readlines()

for i, line in reversed(list(enumerate(lines))):
  if line.endswith("\\"):
    lines[i] += lines.pop(i+1)

for i, line in enumerate(lines):
  if not line.startswith("#"):
    break

for i, line in enumerate(lines[i:], i):
  if line.strip():
    lines.insert(i, "\ndef run(ctx, args):\n")
    break

for i, line in enumerate(lines[i+1:], i+1):
  if line.strip():
    line = "  " + line
  if line.strip().startswith("#"):
    lines[i] = line
    continue
  for exp, sub in {
    "\$\?": 'exit_code',
    "^( *)?echo (.*)": r"\1print(\2)",
    '\$\{@\}': '$@',
    '"\$@"': '$@',
    r'(?:exec )?(?:\$\{RUN\}|./default-run)( .+)? \$@( .+)?\n': r'ctx.default_run(args\1\2)\n',
  }.items():
    line = re.sub(exp, sub, line)
  for flag in ("--android-runtime-option --runtime-option -Xcompiler-option --args --dex2oat-timeout "
               "--vdex-filter --secondary-class-loader-context --dex2oat-rt-timeout").split():
    line = re.sub(r' *(?<![/\-"\'])-+' + flag.strip("-") + r' ([^ \n\\\)]+)(?<!,)',
                  ', ' + flag.strip("-").replace("-", "_") + r'=["\1"]', line)
  for flag in ("--jvmti --vdex --profile --zygote --dex2oat-dm --runtime-dm --jit "
               "--no-secondary-app-image --verify-soft-fail --no-prebuild --no-app-image "
               "--no-image --baseline --external-log-tags --compiler-only-option "
               "--no-verify --secondary "
               "--no-secondary-compilation --sync --no-relocate --add-libdir-argument").split():
    line = re.sub("(?<![`\*]) *" + flag + "(?!-)",
                  ", " + flag.strip("-").replace("-", "_") + "=True", line)
  line = re.sub('(ctx.default.run\() *\,? *', r'\1', line)
  lines[i] = line

with open(sys.argv[1], "wt") as f:
  f.writelines(lines)
