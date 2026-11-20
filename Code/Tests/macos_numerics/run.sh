#!/bin/sh
# [TEMP] [CHOSEN, not recovered] Standalone M2 bundle; no compiler required.
set -u
cd "$(dirname "$0")" || exit 1
exec > results.txt 2>&1
date -u
sw_vers
numeric_machine=$(uname -m)
printf 'Machine: %s\n' "$numeric_machine"
if [ "$numeric_machine" != arm64 ]; then
    printf '%s\n' 'FAIL: run this bundle from a native arm64 shell on the M2.'
    exit 1
fi
numeric_status=0
for numeric_test in numerics portable_x87 portable_x87_exact bitstream_numeric; do
    printf '\nRunning %s\n' "$numeric_test"
    "./$numeric_test"
    numeric_rc=$?
    printf 'Exit: %s\n' "$numeric_rc"
    if [ "$numeric_rc" -ne 0 ]; then numeric_status=1; fi
done
printf '\nOverall exit: %s\n' "$numeric_status"
exit "$numeric_status"
