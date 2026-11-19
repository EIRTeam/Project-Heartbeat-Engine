# Godot 4.7.2: PH integration validation

## Status

Stage 5 is in progress. The integrated Linux development build, headless
module/API smoke test, headless editor startup, and nine focused PH regression
tests passed. The broad portable suite exposed two failing cases described
below. The Linux release-template build is running. The earlier standalone
engine results are recorded in `godot-4.7.2-rebase.md`.

Work is on `upgrade/godot-4.7.2` in `/tmp/opencode/ph-godot-4.7.2`.
The original user checkout is untouched. No stage 5 changes have been pushed.

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

Compatibility changes are committed separately:

- `f222f30874`: engine SDL clock-helper signature.
- `2b641f6718`: external modules and regression tests.

The clock-helper signature change is an engine delta beyond the stage 4
import and must be synchronized back to the versioned Shinobu branch after
validation. The engine no longer exactly matches the earlier published SHA.
No stage 5 changes have been pushed.

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
   `set_action_name`). The development binary must be rebuilt and this test
   rerun before marking it fixed by validation.
2. GDScript's `call_native_static_method_validated.gd` calls
   `Node.print_orphan_nodes()` and expects no output. PH's existing
   `PHAudioStreamPreviewGenerator` and `PHNative` singleton Nodes are
   intentionally unattached and appear in the output. The standalone engine
   does not register these modules and passed the fixture. Do not modify
   expected upstream output or change singleton lifecycle merely to hide this
   discrepancy; record it as a remaining integration-test limitation.

The focused PH regression run completed successfully:

```bash
# Same isolated XDG environment as the broad run.
./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test --no-colors \
  '--test-case=*PHZip*,*FFmpeg*,*InputGlyphs*,*PHNative*'
```

9 cases and 47 assertions passed, zero failures.
Log: `/tmp/opencode/ph-godot-4.7.2-regressions.log`.

The separate release-template build uses:

```bash
scons profile=../misc/build_profiles/production_linux.py \
  debug_symbols=no separate_debug_symbols=no lto=none -k -j2
```

This exercises the release/template compilation paths, but disables symbols
and LTO for local resource limits. It does not validate release symbol uploads
or the unmodified production profile. Log:
`/tmp/opencode/ph-godot-4.7.2-production-build.log`.

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
| Portable integrated engine/module tests | 1,393 passed, 2 failed; binding fixes await retest, orphan-output fixture remains |
| PH ZIP regressions | Passed in focused 9-case regression run |
| FFmpeg converter lifetime regressions | Passed in focused 9-case regression run |
| Linux production release template | Running with symbols/LTO disabled |
| Headless editor startup and module registration | Passed |
| Windows editor/release template | Blocked locally: MinGW cross-compiler not found |
| Android template/APK | Blocked locally: SDK/NDK environment and Android FFmpeg dependencies unavailable |
| Actual Project Heartbeat import/gameplay/export | Game project not present in this checkout |
| Audio timing, devices, latency, seeking | Requires runtime/media/device checks |
| Video playback and teardown/reload | Pending runtime/media checks |
| Diegetic GUI, clipping, Compositor2D layers/masks | Requires rendering validation |
| Linux/Windows minidumps and symbolication | Pending; debug-symbol artifacts/tooling required |

No display server is configured in this shell (`DISPLAY` and `WAYLAND_DISPLAY`
are unset). `vulkaninfo --summary` does detect an AMD RX 7800 XT Vulkan device,
but emits loader/device warnings. This alone does not validate any GPU path.
No Xvfb executable was found. Do not mark rendering checks passed based only
on a dummy/headless renderer.
