# Godot stable upgrade: downstream patch inventory and rebase

## Scope and status

Step 3 is complete: all 12 downstream engine commits have been replayed onto
Godot 4.7.2, and standalone build, tests, and headless editor smoke validation
passed. This is not yet a validated monorepo integration. No remote pushes,
shared `ph` updates, or subrepo imports have been performed.

- Checkout: `/tmp/opencode/shinobu-godot-4.7.2`
- Preserved original branch: `staging/ph-pre-godot-4.7.2`
- Rebased branch: `rebase/ph-godot-4.7.2`
- Original engine tip: `8e748a0209a207666607441e552ad21e4adfa711`
- Upstream target: `ed1daf0bf001b61586d9930840f2f1394092c079`
- Replayed series tip: `dbc6a9094db02cc0feca48e30abfd4f0c660808a`.
- Validation fixes committed on top: `5b3d16461277f6dbbb8ce57c2346bdb5eaff4c1e`
  (`Adapt downstream includes and version metadata to Godot 4.7.2`).
- Recovery ref in the original monorepo:
  `refs/backup/godot-4.7.2/rebased-engine`.

## Rebase boundary

The merge base with the target is Godot 4.6,
`89cea143987d564363e15d207438530651d943ac`.
However, the original fork also contains official 4.6.1 backports ending at
`7bfffc846d`, followed by the 12 custom commits.

`git cherry` confirms that all 34 non-merge functional commits in that
4.6-to-4.6.1-backport range have patch equivalents in the target.
The only two non-equivalent non-merge commits are the old 4.6.1-rc version bump
and translation synchronization; both are superseded by the target release.
Therefore the replay boundary excludes `7bfffc846d` and its ancestors rather
than replaying old release maintenance over the newer upstream.

```bash
git switch -c rebase/ph-godot-4.7.2 staging/ph-pre-godot-4.7.2
git rebase --onto refs/backup/godot-4.7.2/upstream 7bfffc846d
```

## Patch inventory

No downstream commits were dropped.

| Original | Replayed | Decision / adaptation |
| --- | --- | --- |
| `beaf4a65b9` | `f2cb924f2b` | Keep diegetic GUI. Adapt rendering signatures to `RSE` / `RenderingServerTypes`, preserve upstream includes and per-canvas profiling, attach stencil texture after the newer unconditional backbuffer allocation. |
| `d24c82708c` | `0d0e9a9b5b` | Keep SDL timestamping and helper APIs. Preserve upstream joypad event filtering and motion-sensor processing alongside the timestamp arguments. |
| `5038e9ae73` | `b17ec3b3e5` | Keep PH branding, Windows resources, and fork README. |
| `1bdb3320fc` | `228727d074` | Keep Breakpad integration. Retain upstream profiler options and Perfetto ignore rule alongside the added Breakpad settings. |
| `835ce68f18` | `dc0060c473` | Keep Windows Breakpad build fixes against updated platform configuration. |
| `e0e4c3a505` | `1dd7733d21` | Keep clip-plane buffer timing fix; patch unchanged. |
| `ce4ed1885e` | `d5b2b22121` | Keep Compositor2D and callback hooks. Adapt registration include paths to current upstream style. |
| `5e713bd17d` | `542fb532ff` | Keep Vulkan stencil-reference command and render-graph plumbing; patch unchanged. |
| `f616375020` | `67e447a91a` | Keep existing Windows configuration workaround; patch unchanged. |
| `6fdbc63220` | `d0a1108eda` | Keep minidump build fix; patch unchanged. |
| `1bf8ac1f9f` | `fe03ddb159` | Keep minizip 64-bit seek fixes on top of newer upstream minizip. Convert PH ZIP tests to `.cpp` plus `TEST_FORCE_LINK`, following upstream's new test discovery instead of restoring removed test-header includes. |
| `8e748a0209` | `dbc6a9094d` | Keep crash-handler fixes and vendored Breakpad update; patch unchanged. |

## Review and static verification

- `git range-diff` reviewed: all 12 original commits map to replayed commits.
  Five patches are unchanged; the remaining seven reflect the adaptations
  above and upstream context changes.
- Range-diff: `/tmp/opencode/shinobu-godot-4.7.2-range-diff.txt`.
- `version.py` reports `4.7.2-stable` with docs version `4.7`.
- Vendored `thirdparty/breakpad/` and `modules/breakpad/` match the original
  consolidated engine tree exactly.
- `git diff --check` passes outside vendored Breakpad. Breakpad retains its
  pre-existing third-party whitespace warnings; no wholesale formatting was
  applied to vendor sources.
- Rebase completed, with no unresolved conflicts and a clean source checkout
  before starting the build.

## Standalone build validation

```bash
scons platform=linuxbsd target=editor dev_build=yes debug_symbols=no \
  tests=yes use_breakpad=yes sdl=yes scu_build=yes -j2
```

Log: `/tmp/opencode/shinobu-godot-4.7.2-engine-build.log`.
Initial build: failed while compiling Compositor2D because `OS` was not
declared. Added a direct `core/os/os.h` include rather than relying on the
old transitive include chain.

First retry: failed compiling `render_canvas_data_rd.cpp` because the
`ClassDB` definition and `D_METHOD` were no longer included transitively.
Added a direct `core/object/class_db.h` include.

Second retry: succeeded. The resulting binary exposed stale PH-specific
version metadata: `methods.py` imports `ph_version.py`, not `version.py`.
Updated the PH version to 4.7.2 and docs to 4.7, retaining its existing `dev`
status and branding. This also corrects the previous baseline explanation:
the baseline's `dev` version comes from PH metadata, not `dev_build` itself.

Version-corrected rebuild: succeeded. Binary reports
`4.7.2.dev.custom_build.dbc6a9094`; it was built with the validation fixes
before committing them as `5b3d164612`, so its embedded commit hash refers
to the preceding commit.
Log: `/tmp/opencode/shinobu-godot-4.7.2-engine-build-final.log`.
Retry log: `/tmp/opencode/shinobu-godot-4.7.2-engine-build-retry-1.log`.
Second retry log: `/tmp/opencode/shinobu-godot-4.7.2-engine-build-retry-2.log`.

This deliberately builds the engine and its built-in modules without the
monorepo's external modules, to isolate engine rebase errors from later
integration work. Debug symbols are disabled to limit temporary storage use.
The PH ZIP tests remain conditional on `MODULE_HBNATIVE_ENABLED` and cannot
exercise their bodies in this standalone configuration.

Headless editor startup/import/shutdown on the isolated minimal project:
passed (exit 0), no logged errors.
Log: `/tmp/opencode/shinobu-godot-4.7.2-smoke.log`.

The first test invocation did not run tests: the runner logged a null
`DirAccess` error and returned 0 because its isolated XDG cache parent did not
exist. This is not a test pass. Created the isolated XDG directories explicitly.
The full standalone test suite then completed successfully (exit 0):

- 1,386 test cases passed, 0 failed, 3 skipped.
- 419,947 assertions passed, 0 failed.
- Doctest summary: `Status: SUCCESS!`.

The output includes engine error/warning messages from exercised test paths;
the actual doctest assertions and summary passed. This does not establish
that every emitted diagnostic is harmless outside the test suite.
Log: `/tmp/opencode/shinobu-godot-4.7.2-tests-retry.log`.

Final source checkout is clean. The original monorepo checkout retains only
its original local changes. The protected rebased-engine ref points at the
final validation-fix commit.

Full PH module compatibility, Windows/Android builds, GPU rendering, input
timing on hardware, and crash-dump validation remain later gates.
