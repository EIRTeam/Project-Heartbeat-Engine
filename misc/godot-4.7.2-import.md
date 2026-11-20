# Godot stable upgrade: monorepo import

## Result

Stage 4 is complete. The validated standalone engine was published to a new
Shinobu branch and imported with git-subrepo into the isolated monorepo
upgrade branch.

- Shinobu remote: `https://github.com/EIRTeam/shinobu.git`
- Published branch: `ph-godot-4.7.2`
- Published commit: `5b3d16461277f6dbbb8ce57c2346bdb5eaff4c1e`
- Monorepo branch: `upgrade/godot-4.7.2`
- Monorepo worktree: `/tmp/opencode/ph-godot-4.7.2`
- Import commit: `96c918d9991a56116b25d82b23f4563db1f3daed`
- Previous monorepo commit: `916a18df47e5ef27b633dc2244aa96616a14b945`

## Operations

The target remote branch did not exist before publication. Published without
force, leaving shared `ph` unchanged:

```bash
# Standalone Shinobu checkout.
git push origin HEAD:refs/heads/ph-godot-4.7.2

# Isolated monorepo checkout.
source /tmp/opencode/git-subrepo-0.4.9/.rc
git subrepo pull engine --branch=ph-godot-4.7.2 --update --force
```

The controlled replacement was safe because stage 2 verified that the
consolidated engine preserved all committed monorepo engine changes, and
stage 3 accounted for all downstream patches. A merge-based import was not
used, avoiding reintroduction of the pre-rebase history.

## Verification

- Remote branch was independently checked with `git ls-remote` and matches
  the published commit.
- Remote `ph` remains at
  `835ce68f18b5a6de87466251c11f03f6bb940d0a`.
- git-subrepo generated `engine/.gitrepo` with the canonical remote,
  branch `ph-godot-4.7.2`, published commit, and the correct pre-import parent.
  It was not hand-edited.
- Imported tracked contents and file modes match the published engine tree:

  ```bash
  git diff --exit-code HEAD:engine \
    refs/backup/godot-4.7.2/rebased-engine -- . ':(exclude).gitrepo'
  ```

  This check was run immediately after the import and returned 0.
- The import commit changes no paths outside `engine/`.
- No external module, dependency script, or build profile was changed.
- The original user checkout and its local edits remain untouched.
- The monorepo upgrade branch has not been pushed.

Stage documentation is committed separately from the engine import.
The local `buildeps` symlink is intentionally not committed.

## Remaining validation

Standalone build, engine tests, and smoke validation are recorded in
`misc/godot-4.7.2-rebase.md`. They do not validate the imported engine with
the external PH modules.

The monorepo worktree still contains ignored build outputs from its old
baseline build. Do not treat those binaries as upgraded artifacts; stage 5
must use clean build outputs before validating module compatibility.

Next: stage 5, external module/build compatibility and the full project
validation matrix. No such compatibility work is included in this import.
