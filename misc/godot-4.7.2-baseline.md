# Godot stable upgrade: baseline

## Scope

Step 1 only: preserve the starting state and validate the existing engine.
No rebase, subrepo import, or remote push has been performed.

## Pinned revisions

| Item | Revision |
| --- | --- |
| Monorepo baseline | `916a18df47e5ef27b633dc2244aa96616a14b945` |
| Recorded Shinobu commit | `835ce68f18b5a6de87466251c11f03f6bb940d0a` |
| Remote Shinobu `ph` tip at preparation | `835ce68f18b5a6de87466251c11f03f6bb940d0a` |
| Godot target tag | `4.7.2-stable` |
| Godot target commit | `ed1daf0bf001b61586d9930840f2f1394092c079` |
| Complete monorepo engine tree (including `.gitrepo`) | `bea07901cb0cc4cc90ab70e607c2b1fdbbc6e6c3` |

The engine reports `4.6.1-rc` before upgrading. The complete engine tree
includes committed monorepo engine changes beyond the recorded Shinobu commit.
The remote revisions were checked directly with `git ls-remote`.

## Recovery anchors

These refs live in the original repository's shared Git database:

- `refs/backup/godot-4.7.2/monorepo`: complete starting monorepo commit.
- `refs/backup/godot-4.7.2/shinobu-ph`: original Shinobu `ph` commit.
- `refs/backup/godot-4.7.2/upstream`: fetched official Godot stable target.
- `refs/backup/godot-4.7.2/engine-tree`: original engine tree object.

The monorepo commit anchors the complete engine snapshot and its history.
These are local refs, not remote backups.

## Isolated environment

- Branch: `upgrade/godot-4.7.2`
- Worktree: `/tmp/opencode/ph-godot-4.7.2`
- git-subrepo: tag `0.4.9`, commit
  `ea108868fe0998c912c411830bfc50dc259cc635`
- Tool checkout: `/tmp/opencode/git-subrepo-0.4.9`
- Enable the tool in a shell:

  ```bash
  source /tmp/opencode/git-subrepo-0.4.9/.rc
  git subrepo version
  git subrepo status engine
  ```

`git subrepo status engine` succeeds and agrees with the pinned metadata.
The original checkout's `.vscode/settings.json`, `.vscode/tasks.json`, and
untracked `crash-fixes-1-12.patch` are untouched and are not part of the baseline.

The worktree uses a symlink to the existing `buildeps/` directory, avoiding
downloads of moving `latest` dependency archives. Build outputs are isolated.
This validates the baseline against the currently installed dependencies, not
a fully reproducible dependency lock.

The worktree and tool checkout are on temporary storage. The backup refs survive
their removal, but logs, uncommitted documentation, and build outputs do not.

## Baseline validation

From the worktree's `engine/` directory:

```bash
scons profile=../misc/build_profiles/debug_linux.py -j2
```

Build log: `/tmp/opencode/ph-godot-4.7.2-baseline-build.log`.

Build status: failed (exit 2) after 38 seconds, before linking.

Existing pre-upgrade blocker:

```text
modules/steamworks/tests/test_steam_matchmaking.h:163:
error: 'class String' has no member named 'parse_utf8'
```

SCons version: 4.11.1, running under Python 3.14.

No source changes were made to work around this failure. Engine tests cannot
run until the test compilation failure is addressed.

A secondary build with tests disabled is being attempted to separate test
compilation failures from engine build/runtime failures:

```bash
scons profile=../misc/build_profiles/debug_linux.py tests=no -j2
```

Secondary log: `/tmp/opencode/ph-godot-4.7.2-baseline-no-tests-build.log`.
Secondary build status: succeeded in 18 minutes 28 seconds. This does not
validate tests.

The resulting binary reports `4.6.1.dev.custom_build` with `--version`.
The source `version.py` says `4.6.1-rc`; PH's build imports `ph_version.py`
instead, which supplies the `dev` status and Project Heartbeat branding.

Headless editor startup and shutdown succeeded (exit 0) on an isolated minimal
project, with no errors in the log:

```bash
timeout 90s env \
  XDG_DATA_HOME=/tmp/opencode/ph-baseline-xdg/data \
  XDG_CONFIG_HOME=/tmp/opencode/ph-baseline-xdg/config \
  XDG_CACHE_HOME=/tmp/opencode/ph-baseline-xdg/cache \
  ./bin/godot.linuxbsd.editor.dev.x86_64 \
  --headless --path /tmp/opencode/ph-godot-baseline-smoke --editor --quit
```

Smoke log: `/tmp/opencode/ph-godot-4.7.2-baseline-smoke.log`.

Baseline disposition: recovery anchors and buildability are established,
and a basic headless smoke test passes. The test-enabled build remains
blocked by the existing Steamworks compilation error. Project Heartbeat
gameplay, graphical rendering, audio, and export behavior are not validated.
Proceeding to step 2 does not imply those checks passed.
