#!/bin/bash
# Run all tests with CAP_SYS_NICE so SCHED_DEADLINE tests work.
#
# As root (CI): capsh is used directly — no sudo needed.
# As non-root (developer): re-execs via sudo+capsh.
#
# In both cases CAP_SYS_NICE must be available in the process's bounding set.
# For Docker-based CI this requires the runner to be configured with:
#   cap_add = ["SYS_NICE"]   (in the runner's config.toml [runners.docker] section)

if [ "$(id -u)" = "0" ]; then
    exec capsh --inh='cap_sys_nice' --addamb=cap_sys_nice -- \
        -c 'exec luajit tests/run_all_tests.lua "$@"' -- "$@"
else
    exec sudo -E capsh --keep=1 --uid="$(id -u)" \
        --inh='cap_sys_nice' --caps='cap_sys_nice+eip' --addamb=cap_sys_nice -- \
        -c 'exec luajit tests/run_all_tests.lua "$@"' -- "$@"
fi
