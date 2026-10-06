# Experimental sandbox

`tools/build-native.sh` builds the ordinary viewer with seccomp and Landlock
**disabled**. Native PDF/package smoke results do not validate sandbox isolation.
`zatura-sandbox` is a separate executable, built when sandbox dependencies are
available. For the combined Linux configuration:

```sh
make configure BUILDDIR=build/sandbox MESON_ARGS='-Dseccomp=enabled -Dlandlock=enabled -Dsynctex=disabled'
make build BUILDDIR=build/sandbox
build/sandbox/zatura-sandbox document.pdf
```

## Thread coverage

Strict Landlock startup requires **ABI 8 or newer** and always supplies
`LANDLOCK_RESTRICT_SELF_TSYNC`. ABI 6 and 7 are rejected, even though they have
other restrictions used by this policy. GTK/GLib can create threads during UI
initialization. The document renderer is created later, when opening a document;
its creation is not the reason for requiring synchronization of existing threads.
Both Landlock and seccomp must synchronize their own policies independently.
See the [kernel Landlock documentation](https://docs.kernel.org/userspace-api/landlock.html).

An explicit seccomp-only build (`-Dlandlock=disabled`) can run on older kernels.
It denies writable opens and filesystem metadata changes, but it does not
provide Landlock's domain restrictions. X11 process isolation remains incomplete.
Landlock builds also require seccomp for metadata syscall restrictions. An
explicit Landlock-without-seccomp configuration is rejected during setup; auto
Landlock is disabled when seccomp is unavailable.

## Descriptor contract

Before locale, UI or plugin initialization, Linux sandbox startup closes all
inherited descriptors above stderr. Read-only standard input files and stdio
pipes/sockets/terminals are retained. Writable filesystem stdio, including shell
redirection to a regular file, is replaced with `/dev/null`. Use a pipe to retain
logs without giving the parser a writable file descriptor:

```sh
zatura-sandbox document.pdf 2>&1 | tee sandbox.log
```

After policy enforcement and before document parsing, startup enumerates
`/proc/self/fd`. A surviving writable linked regular file or unexpected device
causes startup to fail with a nonzero status. Anonymous kernel event handles and
internally created, unlinked shared-memory buffers remain available to GTK.
Failure to inspect descriptors also aborts startup. `/proc` must be mounted.

Seccomp forbids new writable `open`/`openat`, `mkdir`/`mkdirat` and
`fchmod`/`fchmodat` (which can change permissions even on read-only descriptors).
`write`, `writev`, `ftruncate` and `fallocate` remain available for IPC and
anonymous display buffers; their safety depends on the descriptor contract,
not on pretending that Landlock revokes rights on previously opened FDs.
The UI/compositor IPC endpoint is trusted; this is still an experimental
single-process sandbox, not a separate parser process.

## Verification

- `sandbox-fds`: real inherited writer FD plus stdin/stdout/stderr redirection;
  checks `write`, `writev`, `pwrite`, `ftruncate` and `fallocate`, verifies unchanged
  bytes and permissions, checks surviving writer rejection after seccomp, and
  retains usable memfd/eventfd display buffers.
- `landlock-abi-policy`: deterministic syscall-wrapped tests of unsupported ABIs,
  specifically 6 and 7, plus required TSYNC flags on ABI 8 and later. These are
  boundary tests, not proof of kernel runtime enforcement.
- `landlock-threads`: on a real ABI 8+ kernel, starts a worker before installing
  the production Landlock policy. With seccomp absent, that worker must fail
  writable/truncating/creating opens and `mkdir`, while reading still succeeds.
  Older kernels produce an explicit SKIP instead of a false positive result.
- `xvfb_landlock_startup`: refusal on ABI <8; actual PDF render on ABI 8+.

`tools/test-native-sandbox.sh` runs these tests. Set `LANDLOCK_REQUIRE_TSYNC=1`
to require the positive runtime test to pass; a skip then fails verification.
The manually dispatched `landlock-runtime.yaml` workflow applies this requirement
on a self-hosted Linux runner labelled `landlock-abi8`. That runner must actually
provide the kernel feature; Docker does not replace the host kernel. Registering
or starting such a runner is outside this source change.

Before publishing a new release, use a new project version/tag and run the full
engine/Wayland suite, package smoke checks and ABI 8 runtime workflow. Portal
file selection needs its own interactive verification; opening a temporary PDF
with test-only filesystem access is not proof of portal operation.
