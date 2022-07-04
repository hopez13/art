#!/usr/bin/env python3

import os, sys, subprocess, re

cmd = " ".join(sys.argv[1:]).replace("bash -c ", "")
if cmd != "true":
  with open("/tmp/art-log/" + os.environ["FULL_TEST_NAME"], "a") as f:
    tmp = os.environ["DEX_LOCATION"]
    env = dict(sorted(os.environ.items()))
    for k in ["SHLVL", "_", "ART_TOOLS_BUILD_VAR_CACHE", "PWD", "OLDPWD", "TMUX", "TMUX_PANE"]:
      env.pop(k)
    f.write("\n".join(k + ":" + v.replace(tmp, "<tmp>") for k, v in env.items()) + "\n\n")
    f.write(re.sub(" +", "\n", cmd).replace(tmp, "<tmp>") + "\n\n")

if "/dalvikvm" in cmd or cmd.startswith("adb "):
  if "-checker-" not in os.environ["TEST_NAME"]:
    sys.exit(0)
subprocess.run(sys.argv[1:])
