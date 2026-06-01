# Reproducing GitLab CI locally with podman

Run the same `gcc:trixie` container the CI uses, with the repo mounted.
The full CI procedure (apt deps, dep clones, build, tests) is in
`.gitlab-ci.yml`; this note covers the developer-facing knobs.

## Rootless (recommended)

From the repo root:

```sh
podman run --rm -it \
    --cap-add=SYS_NICE \
    -v "$PWD":/work -w /work \
    docker.io/library/gcc:trixie bash
```

Inside the container, follow `.gitlab-ci.yml` (`.setup_deps` → `build` →
`test`) to install deps, build microblx, and run the suite.

The `--cap-add=SYS_NICE` is required for the SCHED_FIFO/RR ptrig tests.

**`TestPtrig:TestDeadlineRuns` will auto-skip under rootless podman.**
SCHED_DEADLINE is rejected in non-initial user namespaces regardless of
`CAP_SYS_NICE`, so the test detects this via `/proc/self/uid_map` and
calls `luaunit.skipIf(...)`. All other tests run unchanged.

## With sudo (covers SCHED_DEADLINE too)

To exercise SCHED_DEADLINE — i.e., to fully match the CI environment —
run podman as root:

```sh
sudo podman run --rm -it \
    --cap-add=SYS_NICE \
    -v "$PWD":/work -w /work \
    docker.io/library/gcc:trixie bash
```

The container then runs in the init user namespace, the kernel allows
`sched_setattr(SCHED_DEADLINE)`, and `TestDeadlineRuns` executes for real.

## Notes

- `docker` works with the same flags — substitute `docker` for `podman`.
  Podman's strict short-name resolution requires the
  `docker.io/library/` prefix; plain `gcc:trixie` will error.
- The setup step is slow (~2 min). For iterative debugging,
  `podman commit <container-id> microblx-ci:trixie` after setup and
  re-run against that tag.
- `dbus-run-session` converts a signal-killed child into exit 1, masking
  SIGSEGV (which would normally be 139). If `test-results.xml` is empty
  or truncated, the runner crashed mid-suite — run without
  `dbus-run-session` to see the real signal.
