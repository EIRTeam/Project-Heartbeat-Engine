# Project Heartbeat debug symbol uploads

The Windows and Linux build workflows call this action after compilation, only
for pushes to `master` and manual runs targeting `master`. Pull requests, merge
queues, tags, and other branches never run the action. Android is unchanged.

Configure the repository Actions secret `GLITCHTIP_AUTH_TOKEN` with a GlitchTip
API token authorized to upload project debug files. The caller explicitly passes
it through each reusable workflow, and only the upload step exposes it as
`SENTRY_AUTH_TOKEN`. The token is not a DSN and must not be committed.

Uploads target `https://glitchtip.eirexe.moe`, organization `eirteam`, project
`project-heartbeat`. Sentry CLI is compatible with GlitchTip's native debug-file
API and chunks large PDB/ELF files to respect the server's upload limits. Its
version and SHA-256 hashes are pinned in `upload.py`; update both together when
upgrading the CLI.

Windows uploads the current build target's PDB files. Linux uploads the original
ELF binary with embedded DWARF before the existing extraction/strip step. Existing
artifact and release packaging are unchanged. Files must exist, be nonempty, and
contain usable debugging information; missing credentials, failed uploads, and
server processing errors fail the master build. The CLI waits up to 10 minutes
for server processing, within the step's 20-minute timeout.

Symbol files are matched to crashes by their debug IDs, not filenames or Git
commit hashes. This action does not configure client-side crash submission.

Run the uploader's unit tests from the repository root:

```sh
python -m unittest discover -s .github/actions/upload-debug-symbols -p 'test_*.py'
```
