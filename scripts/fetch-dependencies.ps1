$ErrorActionPreference = 'Stop'
# Fetch the pinned commits, without downloading each dependency's full history.
# Keep the main repository history: CI diffs and release ancestry checks need it.
git submodule update --init --recursive --depth 1 --jobs 4
if ($LASTEXITCODE -ne 0) { throw 'Fetching pinned native dependencies failed.' }
$status = git submodule status --recursive
if ($LASTEXITCODE -ne 0 -or ($status | Where-Object { $_ -match '^[-+U]' })) {
    throw 'Native dependencies do not match the committed submodule revisions.'
}
