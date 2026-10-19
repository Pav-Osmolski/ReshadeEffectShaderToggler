$ErrorActionPreference = 'Stop'
python scripts/package-build.py --kind test
if ($LASTEXITCODE -ne 0) { throw 'Test package creation failed.' }
