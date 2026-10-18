$ErrorActionPreference = 'Stop'
$commit = git rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Could not identify test build commit.' }
$version = (Select-String src/version.h -Pattern 'REST_VERSION_STRING "([^"]+)"').Matches[0].Groups[1].Value
New-Item -ItemType Directory -Force test-package | Out-Null
$info = @"
REST Enhanced test candidate $version
Commit: $commit
This package is for runtime validation; it is not a published stable release.
See docs/RELEASING.md for the GPU regression matrix, including Warframe issue #15.
CI verifies compilation, binary metadata and CPU regression tests, not game/GPU behavior.
"@
$info | Set-Content test-package/BUILD-INFO.txt -Encoding utf8
$name = "REST-test-$version-$($commit.Substring(0,12)).zip"
$paths = @('src/x64/Release/ReshadeEffectShaderToggler.addon64',
           'src/Win32/Release/ReshadeEffectShaderToggler.addon32',
           'LICENSE', 'README.md', 'CHANGELOG.md', 'Shaders', 'docs', 'test-package/BUILD-INFO.txt')
Compress-Archive -Path $paths -DestinationPath "test-package/$name" -Force
$hash = (Get-FileHash "test-package/$name" -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $name" | Set-Content test-package/SHA256SUMS.txt -Encoding ascii
