#!/bin/sh
# Run the PID example. rt and deadline require CAP_SYS_NICE (sudo+capsh).
# Usage: run-pid.sh [nrt|rt|deadline] [UBX-LAUNCH-ARGS...]  (default: nrt)
# e.g. run-pid.sh rt -D PERIOD=500

MODE=${1:-nrt}
[ $# -gt 0 ] && shift

case "$MODE" in
-h|--help)
    echo "Usage: $(basename "$0") [nrt|rt|deadline] [UBX-LAUNCH-ARGS...]  (default: nrt)"
    echo "  nrt       SCHED_OTHER (no privileges needed)"
    echo "  rt        SCHED_FIFO prio 99  (needs CAP_SYS_NICE)"
    echo "  deadline  SCHED_DEADLINE      (needs CAP_SYS_NICE)"
    echo "further arguments are passed to ubx-launch, e.g. -D PERIOD=500"
    exit 0 ;;
nrt)
    exec ubx-launch -c pid_test.usc,ptrig_nrt.usc -dbus -webgraph "$@" ;;
rt|deadline)
    exec sudo -E capsh --keep=1 --uid="$(id -u)" \
         --inh='cap_sys_nice' --caps='cap_sys_nice+eip' --addamb=cap_sys_nice -- \
         -c 'exec ubx-launch -c pid_test.usc,ptrig_'"$MODE"'.usc -dbus -webgraph "$@"' -- "$@" ;;
*)
    echo "$(basename "$0"): unknown mode '$MODE'" >&2; exit 1 ;;
esac
