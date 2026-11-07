#!/bin/sh
#
# run the C test programs (liblfq, liblfb, rtlog) from PATH, one
# PASS/FAIL/SKIP line per command. Used by run_tests.sh, the CI and
# the Yocto ptests, so this is the one list of C tests.
#
# The cmocka unit tests are SKIPped when not built (no cmocka), unless
# UBX_REQUIRE_UNIT_TESTS=1; the others FAIL when missing. Exits 1 if
# any test failed.

fail=0
log=$(mktemp)

run() {
	if ! command -v "$1" >/dev/null 2>&1; then
		if [ -n "$unit" ] && [ "$UBX_REQUIRE_UNIT_TESTS" != 1 ]; then
			echo "SKIP: $* (not built)"
		else
			echo "FAIL: $* (not found)"
			fail=1
		fi
		return
	fi

	if "$@" > "$log" 2>&1; then
		echo "PASS: $*"
	else
		echo "FAIL: $*"
		sed "s/^/# /" "$log"
		fail=1
	fi
}

unit=1
run test-lfq-unit
run test-lfb-unit
run test-lfb-shm-unit
run test-lfbm-unit

unit=
# lfq: default cap 4 and the single-slot mailbox path
run test-lfq
run test-lfq -c 1 -t 8 -o 50000
# lfb multi-reader stress test, incl. forced overrun recovery
run test-lfb
run test-lfb -r 8 -O
run test-lfbm
run test-lfbm -d 16
# rtlog shm log: defaults, high contention and the SPSC fast path
run test-rtlog
run test-rtlog -w 32 -r 8 -n 50
run test-rtlog -w 1 -r 1 -n 20

rm -f "$log"
exit $fail
