# Godot 4.7.2: PH integration validation

## Status

Stage 5 has passed integrated Linux builds, all 1,395 portable engine cases
(424,953 assertions), actual PH import and 11 game tests, and software Vulkan/GL
rendering/video checks. CI passed all six platform build jobs. Linux CI-symbol
minidump validation recovered the crash function and source line.
This is not release approval: physical audio/GPU gameplay, live services,
Windows crash handling, and fresh-cache export diagnostics remain unvalidated.
Earlier standalone results are in `godot-4.7.2-rebase.md`.

Work is on `upgrade/godot-4.7.2` in `/tmp/opencode/ph-godot-4.7.2`.
The original user checkout is untouched. Upgrade branches have been pushed;
shared `ph` remains unchanged. See the publication section below.

## Clean development build

Moved the baseline's generated `engine/bin/` and SCons state files to
`/tmp/opencode/ph-godot-baseline-artifacts/`, retaining them without reuse.
The existing dependency directory remains linked from `buildeps`.

```bash
cd engine
scons profile=../misc/build_profiles/debug_linux.py debug_symbols=no -j2
```

Debug symbols are disabled to limit temporary-storage use. All seven custom
modules and engine tests remain enabled. This is not an exact debug-symbol
validation of the unmodified profile.

Build logs:

- `/tmp/opencode/ph-godot-4.7.2-modules-build.log`
- `/tmp/opencode/ph-godot-4.7.2-modules-build-retry-1.log`
- `/tmp/opencode/ph-godot-4.7.2-modules-build-retry-2.log`
- `/tmp/opencode/ph-godot-4.7.2-modules-build-retry-3.log`
- `/tmp/opencode/ph-godot-4.7.2-modules-build-retry-4.log`
- `/tmp/opencode/ph-godot-4.7.2-modules-build-retry-5.log`
- `/tmp/opencode/ph-godot-4.7.2-modules-build-retry-6.log`
- `/tmp/opencode/ph-godot-4.7.2-modules-build-retry-7.log`
- `/tmp/opencode/ph-godot-4.7.2-modules-build-retry-8.log`
- `/tmp/opencode/ph-godot-4.7.2-modules-build-retry-9.log`
- `/tmp/opencode/ph-godot-4.7.2-modules-build-retry-10.log`
- `/tmp/opencode/ph-godot-4.7.2-modules-build-retry-11.log` (used `-k` to collect
  independent compilation failures in one pass)
- `/tmp/opencode/ph-godot-4.7.2-modules-build-retry-12.log`: succeeded.

The integrated development binary reports `4.7.2.dev.custom_build`.

## Compatibility changes so far

- Replace the removed `String::parse_utf8` call in the Steamworks chat test
  with length-aware `String::utf8`.
- Include `class_db.h` and `callable_mp.h` directly where used, rather than
  relying on includes removed by Godot's header cleanup.
- Include `scene_tree.h` directly in the Steam glyph-dump implementation.
- Update RmlUi cursor enums to `DisplayServerEnums` and rendering enums to
  `RSE`, with direct enum-header includes.
- Include the joypad enum definitions directly and extend the input-glyph
  lookup for Godot's new `MISC2` through `MISC6` buttons. These have no defined
  PH glyph origin, so map them to `INPUT_ORIGIN_INVALID`. Preserve existing
  button mappings and retain the full-size compile-time assertion.
- Add regression tests for existing and additional SDL glyph mappings.
- Adapt input-glyph SVG rendering to ThorVG's new raw-paint ownership,
  canvas `add`/`draw(true)` API, and `ColorSpace::ABGR8888S`. Follow the
  upstream SVG loader and use an RAII pixel buffer instead of manual allocation
  and channel conversion. Add a rasterization size/red-blue-channel regression.
- Remove the obsolete `callable_method_pointer.h` include and use
  `callable_mp.h`; include enum `type_info.h` and rendering definitions directly.
- Godot removed the SDL driver singleton. Query connected device GUIDs and
  SDL's `mapping_handled` flag through `Input` rather than restoring that
  singleton. Make the existing engine SDL clock helper static so PH's clock
  remains in the same time domain as SDL event timestamps. Add clock/default
  device-query tests.
- Qualify the rectangle packer's helper with its new `Math` namespace and
  include the texture resource definition in RmlUi's render-interface header.
- Include Compositor2D dependencies directly so non-SCU release compilation
  does not depend on neighboring translation units.
- Guard deferred editor script-doc-cache work against editor destruction and
  command-line mode, fixing the export-shutdown crash observed below.

Compatibility changes are committed separately:

- `f222f30874`: engine SDL clock-helper signature.
- `2b641f6718`: external modules and regression tests.

These engine deltas have been synchronized to Shinobu's versioned branch.
The later Vulkan video fix removes CPU push-constant padding so its 4-byte
layout matches the shader; a static assertion and GPU fixture cover it.

## Test safety and environment

Do not blindly run every existing custom-module test:

- Steamworks tests require an installed/running Steam client, account/app
  access, and live service operations, including matchmaking/UGC.
- DIVA tests read hard-coded personal game-asset paths and write files there.
  They must not be run as portable tests or redirected into user data.

The portable test run will exclude these dependency-bound tests explicitly,
while retaining PH ZIP, FFmpeg ownership/lifetime, SVG, glyph, and input-clock
regression tests. The current run uses:

```bash
timeout 300s env \
  XDG_DATA_HOME=/tmp/opencode/ph-integrated-tests-xdg/data \
  XDG_CONFIG_HOME=/tmp/opencode/ph-integrated-tests-xdg/config \
  XDG_CACHE_HOME=/tmp/opencode/ph-integrated-tests-xdg/cache \
  ./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test --no-colors \
  '--test-case-exclude=*Steam*,*DIVAMotion*'
```

The XDG directories were created before starting the runner.
Log: `/tmp/opencode/ph-godot-4.7.2-integrated-tests.log`.
Initial portable result: 1,395 cases, 1,393 passed, 2 failed, 23 skipped;
424,955 assertions, 9 failed. The two failing cases are:

1. ClassDB exposed-method validation reports seven unnamed arguments in
   FFmpeg, InputGlyphRect, and PHNative bindings. Added explicit `D_METHOD`
   argument names (and corrected a mis-bound default argument in
   `set_action_name`). Development rebuild and retest passed ClassDB validation.
2. GDScript's `call_native_static_method_validated.gd` calls
   `Node.print_orphan_nodes()` and expects no output. PH's existing
   `PHAudioStreamPreviewGenerator` and `PHNative` singleton Nodes are
   currently unattached and appear in the output. The standalone engine
   does not register these modules and passed the fixture. Do not modify
    expected upstream output or change singleton lifecycle merely to hide this
    discrepancy. Instead, the fixture now exercises a native static void setter
    using its current value, independent of ambient orphan Nodes.

Retest after the binding fixes:
1,395 cases, 1,394 passed, 1 failed, 23 skipped;
424,953 assertions, 424,951 passed, 2 failed. Both failed assertions are in
the GDScript fixture case above; ClassDB no longer fails.
Log: `/tmp/opencode/ph-godot-4.7.2-integrated-tests-final.log`.

The focused PH regression run completed successfully:

```bash
# Same isolated XDG environment as the broad run.
./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test --no-colors \
  '--test-case=*PHZip*,*FFmpeg*,*InputGlyphs*,*PHNative*'
```

9 cases and 47 assertions passed, zero failures.
Log: `/tmp/opencode/ph-godot-4.7.2-regressions.log`.

After the final editor fix, a combined ClassDB/PH regression run passed:
10 cases, 154,494 assertions, zero failures. Filter:
`--test-case=*ClassDB*,*PHZip*,*FFmpeg*,*InputGlyphs*,*PHNative*`.
Log: `/tmp/opencode/ph-godot-4.7.2-final-targeted-tests.log`.
The final portable rerun after the fixture change passed all 1,395 cases and
424,953 assertions, with 23 skipped. Expected fixture output and singleton
ownership are unchanged.
Log: `/tmp/opencode/ph-godot-4.7.2-integrated-tests-fixture-fixed.log`.

The separate release-template build uses:

```bash
scons profile=../misc/build_profiles/production_linux.py \
  debug_symbols=no separate_debug_symbols=no lto=none -k -j2
```

This exercises the release/template compilation paths, but disables symbols
and LTO for local resource limits. It does not validate release symbol uploads
or the unmodified production profile. Log:
`/tmp/opencode/ph-godot-4.7.2-production-build.log`.

The initial release build failed only in Compositor2D, where the non-SCU
compilation path exposed missing direct includes for the rendering server,
callable helpers, and ClassDB. Added those includes to the engine source.
Retry passed with the same options; log:
`/tmp/opencode/ph-godot-4.7.2-production-build-retry-1.log`.
Development rebuild then passed, followed by the ClassDB retest above.
SCons builds were run sequentially to avoid concurrent writes to generated files.

## Export and packed release startup

Production disables loose-project path overrides, as expected. Attempts using
`--path` or the project CWD were rejected; the build settings were not weakened.

Exported the synthetic smoke project to a PCK, copied the release executable
next to `smoke.pck`, and linked its FFmpeg/Steam runtime libraries in the
isolated `/tmp/opencode/ph-packed-release-smoke` directory. Running the packed
release with `--headless` returned 0 and printed `PH_RELEASE_SMOKE_PASS`.
Log: `/tmp/opencode/ph-godot-4.7.2-release-packed-smoke.log`.

The first PCK export wrote its output but aborted during shutdown (exit 134),
logging a null EditorNode singleton. Later exports, including under GDB,
returned 0, so successful retries alone were not treated as a fix.

The captured core and linker map identify the crashing thread in:

```text
EditorHelp::_regen_script_doc_thread
EditorHelp::_reload_scripts_documentation
EditorFileSystemDirectory::get_subdir_count
```

A deferred doc-cache request can run after EditorNode destruction, interpret
the null-singleton `is_cmdline_mode()` fallback as false, and start work with a
destroyed filesystem. Guard both script-doc-cache entry points against an
absent editor and command-line mode.

The updated development build succeeded, followed by three exports using
separate fresh project directories and XDG caches. All returned 0 with no
`ERROR`/script-error diagnostics:

- `/tmp/opencode/ph-godot-4.7.2-export-fixed-1.log`
- `/tmp/opencode/ph-godot-4.7.2-export-fixed-2.log`
- `/tmp/opencode/ph-godot-4.7.2-export-fixed-3.log`

These were synthetic projects, not Project Heartbeat itself.

Diagnostics (temporary, not committed):

- `/tmp/opencode/ph-godot-4.7.2-export-pack.log`
- `/tmp/opencode/ph-godot-4.7.2-export-pack-gdb.log`
- `/tmp/opencode/ph-godot-export-core-backtrace.log`
- `/tmp/opencode/ph-godot-4.7.2-development-link.map`
- `/tmp/opencode/ph-godot-4.7.2-export-fix-build.log`

## Headless smoke checks

The isolated `/tmp/opencode/ph-modules-smoke` project passed a script checking:

- Version 4.7.2.
- Steamworks, Shinobu, InputGlyphsSingleton, RmlUi, PHNative, and DiscordRPC
  singleton registration (without authenticating to external services).
- FFmpeg, PH ZIP, glyph, RmlUi, Shinobu, and compositor class registration.
- Input-event timestamp round trip and CanvasLayer's 3D-transform flag.

Exit 0 with marker `PH_MODULE_SMOKE_PASS`.
Log: `/tmp/opencode/ph-godot-4.7.2-module-smoke.log`.
Headless editor startup/import/shutdown also returned 0 with no errors in
`/tmp/opencode/ph-godot-4.7.2-editor-smoke.log`.

## Validation matrix

| Check | Status / prerequisite |
| --- | --- |
| Linux development build with all custom modules | Passed with debug symbols disabled |
| Portable integrated engine/module tests | All 1,395 cases / 424,953 assertions passed; 23 skipped |
| PH ZIP regressions | Passed in focused 9-case regression run |
| FFmpeg converter lifetime regressions | Passed in focused 9-case regression run |
| Linux production release template | Build and packed startup passed with symbols/LTO disabled |
| Synthetic PCK export | Lifecycle fix passed three fresh-project exports |
| Headless editor startup and module registration | Passed |
| Windows editor/debug/release template | Passed CI builds; runtime not tested |
| Android release template | Passed CI build; APK/device runtime not tested |
| Actual Project Heartbeat import/tests | Clean import and 11/11 tests |
| Actual Project Heartbeat export | PCK produced; repeat export clean, fresh-cache diagnostics unresolved |
| Audio timing, devices, latency | Requires physical device checks |
| Video playback, seeking, teardown | Passed software Vulkan and GL, three cycles each |
| Clipping / Compositor2D | Pixel clipping passed both; callback/textures passed Vulkan; gameplay/layers/masks remain |
| Linux minidump symbolication | CI binary/symbols recovered crash function/source; exception metadata incomplete |
| Windows minidumps | Runtime not tested |

No display server is configured in this shell (`DISPLAY` and `WAYLAND_DISPLAY`
are unset). `vulkaninfo --summary` does detect an AMD RX 7800 XT Vulkan device,
but emits loader/device warnings. This alone does not validate any GPU path.
An isolated downloaded Xvfb plus Mesa software drivers subsequently enabled
real Vulkan/GL code-path checks. AMD presentation under Xvfb failed due to
missing DRI3; no physical-GPU pass is claimed.

## Handoff

Engine fixes remain separate from custom-module fixes. The original checkouts
retain their original local edits.

Before landing/release:

1. Verify CI on the final pushed revision (prior green CI predates the video fix).
2. Repeat actual-game export with a fresh import cache and investigate the
   first-run legacy-resource, shader, and tool-scene diagnostics.
3. Validate actual gameplay/audio/GUI on supported hardware and live services.
4. Test Windows crash handling/symbolication and Linux exception metadata.

Do not treat the successful local Linux checks as completion of this matrix.

## Actual PH project validation

Isolated game checkout:
`/home/eirexe/hdd_backup/repos/Project-Heartbeat-godot-4.7.2-validation`,
branch `upgrade/godot-4.7.2-validation`, based on `508324c1c0e5`.
The original game checkout and its local settings are unchanged.
XDG data/config/cache use `/tmp/opencode/ph-game-validation-xdg/`.
An isolated override disables Steam auto-init; import/export use
`-- --gdscript-tests` to avoid service initialization.

Compatibility fixes explicitly return bool from two `_set` hooks and
`EditorLayer._can_drop_data`, preserving default property handling. Regression
tests cover these hooks. Test mode disables Diagnostics processing before
Shinobu initialization and parents the two otherwise orphaned timers without
starting them. The clean result is 11/11 tests, zero runner errors and no leaks:
`/tmp/opencode/ph-game-4.7.2-tests-clean.log`.
Import log: `/tmp/opencode/ph-game-4.7.2-import-retry.log`.

Actual `Main` export produced a roughly 63 MiB PCK in
`/home/eirexe/hdd_backup/repos/ph-godot-4.7.2-validation-artifacts/`.
Six ignored downloader dependencies were copied from the original checkout
into this isolated checkout for the export plugin; originals are untouched.
The first export exited 0 but logged missing export-plugin `_get_name`,
an InputManagerTester type-inference failure, obsolete GDNative resources,
shader sampler checks, missing glyph children and child-order errors.
Added the required plugin name and explicit float type. Repeat export exited
0 with no error diagnostics, but reused import/export caches and does not
prove the other first-run diagnostics resolved.
Logs: `/tmp/opencode/ph-game-4.7.2-export.log` and
`/tmp/opencode/ph-game-4.7.2-export-final.log`.
Actual packed-game startup/gameplay is not claimed.

## Renderer/media and crash validation

Reproducible fixture: `misc/validation/godot_upgrade_gpu/` (README includes
clip generation and commands). Both tracked-path runs returned 0 and
`PH_GPU_MEDIA_PASS`. GL logs only the expected Xvfb V-Sync warning.
Coverage: clipping pixels, rendered decoded-red frame, seek advancement,
three playback/teardown cycles and surviving texture wrappers; Vulkan also
checks Compositor2D callbacks and screen/backbuffer textures.
Logs: `/tmp/opencode/ph-gpu-tracked-vulkan.log` and
`/tmp/opencode/ph-gpu-tracked-gles.log`.
Local development rebuild after the push-constant fix passed:
`/tmp/opencode/ph-godot-4.7.2-vulkan-video-fix-build.log`.

The CI Linux editor intentionally crashed via `PHNative.crash_test()` and
produced a minidump. Matching `.syms` (debug ID
`81A5E241FB7AF67347A8D61A250AAA0D0`) and checksum-verified
minidump-stackwalk 0.27.0 recovered `PHNative::crash_test()` and
`modules/hbnative/ph_singleton.cpp:20`. The handler uses
`SimulateSignalDelivery`; exception type remains
`unknown 0x00000000 / 0x00000000`. Function recovery is not complete
exception-metadata validation.
Result: `/tmp/opencode/ph-linux-crash-symbolicated.json`.

## Publication and CI

Initial integrated CI:
https://github.com/EIRTeam/Project-Heartbeat-Engine/actions/runs/44557184504
passed Linux editor/release, Windows editor/debug/release and Android release.
Normal CI symbols were built; public release publication was safely skipped
by the new master-only guard. This run predates the final video/fixture fixes.

`git subrepo push engine` synchronized Shinobu `ph-godot-4.7.2` to
`cf26cf1712e6c12d3569a2aeff0245166665b8f7`; engine tree matches except
for monorepo `.gitrepo` metadata. Remote shared `ph` remains
`835ce68f18b5a6de87466251c11f03f6bb940d0a`.
