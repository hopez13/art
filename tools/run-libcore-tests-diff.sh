for mode in host device jvm; do
for gcs in "" " --gcstress"; do
for dbg in "" " --debug"; do
for etc in "" " --no-getrandom" "--no-jit"; do
  args="--mode=$mode$gcs$dbg --dry-run"
  echo "# $args"
  diff --unified <(art/tools/run-libcore-tests.sh $args | tr ' ' '\n') <(art/tools/run-libcore-tests.py $args | tr ' ' '\n')
done
done
done
done
