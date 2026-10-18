$ErrorActionPreference = 'Stop'
function Assert-Parses([string]$Source, [string]$Label) {
    $parseErrors = $null
    $tokens = $null
    [void][System.Management.Automation.Language.Parser]::ParseInput($Source, [ref]$tokens, [ref]$parseErrors)
    if ($parseErrors.Count) { throw "$Label has PowerShell syntax errors: $parseErrors" }
}
Get-ChildItem scripts, src -Filter *.ps1 -Recurse | ForEach-Object {
    Assert-Parses (Get-Content $_.FullName -Raw) $_.FullName
}
# Python emits each pwsh run block as one JSON string to preserve its newlines.
$blocks = python -c 'import json,pathlib,yaml; [print(json.dumps(s["run"])) for p in pathlib.Path(".github/workflows").glob("*.yml") for j in yaml.safe_load(p.read_text())["jobs"].values() for s in j.get("steps",[]) if s.get("shell")=="pwsh" and "run" in s]'
if ($LASTEXITCODE -ne 0) { throw 'Could not read workflow scripts.' }
foreach ($block in $blocks) {
    $source = ($block | ConvertFrom-Json) -replace '\$\{\{.*?\}\}', 'placeholder'
    Assert-Parses $source 'workflow step'
}
Write-Output 'PowerShell syntax validation passed.'
