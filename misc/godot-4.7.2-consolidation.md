# Godot stable upgrade: engine consolidation

## Result

Step 2 is complete locally. No rebase or remote push has been performed.

- Standalone Shinobu checkout: `/tmp/opencode/shinobu-godot-4.7.2`
- Staging branch: `staging/ph-pre-godot-4.7.2`
- Consolidated commit: `8e748a0209a207666607441e552ad21e4adfa711`
- Consolidated tree: `dc108b1c3bb246f5514454d98e1b2f425f6e23e6`
- Origin: `https://github.com/EIRTeam/shinobu.git`
- Fetched remote `ph`: `835ce68f18b5a6de87466251c11f03f6bb940d0a`

## Export and reconciliation

From the isolated monorepo worktree:

```bash
source /tmp/opencode/git-subrepo-0.4.9/.rc
git subrepo fetch engine
git subrepo branch engine
```

The fetched remote `ph` matches the recorded `.gitrepo` commit, so there
is no remote divergence to reconcile. The exported `subrepo/engine` branch
contains seven additional engine-only commits, preserving their order:

| Exported commit | Subject |
| --- | --- |
| `e0e4c3a505` | Fix clipping plane buffer being one frame late. |
| `ce4ed1885e` | Cherry-pick Compositor2D PR |
| `5e713bd17d` | Implement most of layer stuff and clipmasks. |
| `f616375020` | The nuclear option |
| `6fdbc63220` | Fix minidump build |
| `1bf8ac1f9f` | Fix more crashes |
| `8e748a0209` | Fix crash handler and update breakpad |

This includes the ZIP changes/tests and Breakpad third-party updates, not
just the rendering features suggested by commit subjects.

The standalone checkout was cloned locally from this export, with shared
objects, then pointed to the real Shinobu origin and given a dedicated staging
branch. It depends on the original monorepo object database; it is not an
independent off-machine backup. No new synthesized/squashed source commits
were needed.

## Verification

All checks passed:

1. `origin/ph` is an ancestor of the staging branch.
2. The standalone staging checkout is clean.
3. The exported tree matches the original monorepo engine tree exactly,
   excluding only `.gitrepo`, including Git file modes and object contents:

   ```bash
   git diff --exit-code \
     refs/backup/godot-4.7.2/monorepo:engine \
     refs/backup/godot-4.7.2/consolidated-engine \
     -- . ':(exclude).gitrepo'
   ```

4. The original user checkout still has only its original local changes.

The consolidated history is also protected in the original monorepo by
`refs/backup/godot-4.7.2/consolidated-engine`, independent of the temporary
subrepo branch and standalone checkout.

External custom modules remain in the monorepo; they are not part of this
engine-only export. Their compatibility work belongs to the later integration
steps. The untracked user patch is likewise not part of the committed baseline.

## Next gate

Step 3 must inventory the full downstream patch series against official Godot
history before choosing the rebase boundary. The seven commits above are only
the monorepo additions after the recorded Shinobu revision, not the entire
downstream patch series.
