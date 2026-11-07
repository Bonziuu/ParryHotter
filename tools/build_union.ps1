# Build the exact local plugin with the pinned Gothic I SDK. Requires Visual
# Studio 2022 C++ desktop tools (Win32) and Git. No release is published.
[CmdletBinding()]
param([string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repoRoot '.parryhotter/build' }
$sdkCommit = '4ae1fc51bd962e72ed2a0b1d3da92084517db996'
$msbuild = Get-Command msbuild.exe -ErrorAction SilentlyContinue
if ($msbuild) { $msbuildPath = $msbuild.Source }
else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw 'Install Visual Studio 2022 Build Tools with Desktop development with C++ and Windows SDK.' }
    $msbuildPath = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild/**/Bin/MSBuild.exe' | Select-Object -First 1
    if (-not $msbuildPath) { throw 'MSBuild was not found in Visual Studio.' }
}
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('parryhotter-sdk-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary | Out-Null
$previousCL = $env:CL
try {
    git -C $temporary init --quiet
    if ($LASTEXITCODE -ne 0) { throw 'SDK Git initialization failed.' }
    git -C $temporary remote add origin https://github.com/Gratt-5r2/UnionProject.git
    if ($LASTEXITCODE -ne 0) { throw 'SDK origin configuration failed.' }
    git -C $temporary fetch --depth 1 origin $sdkCommit
    if ($LASTEXITCODE -ne 0) { throw 'Pinned SDK download failed.' }
    git -C $temporary checkout --detach FETCH_HEAD
    if ($LASTEXITCODE -ne 0 -or (git -C $temporary rev-parse HEAD) -ne $sdkCommit) { throw 'SDK commit verification failed.' }
    $pluginDirectory = Join-Path $temporary 'UnionProject/Plugin'
    Copy-Item (Join-Path $repoRoot 'union/Plugin/plugin.cpp') $pluginDirectory -Force
    Copy-Item (Join-Path $repoRoot 'union/Plugin/*.hpp') $pluginDirectory -Force
    Copy-Item (Join-Path $repoRoot 'union/hud/*.hpp') $pluginDirectory -Force
    Copy-Item (Join-Path $repoRoot 'union/userapi/*.inl') (Join-Path $temporary 'UnionProject/GothicAPI/Gothic_UserAPI') -Force
    # The pinned SDK's version-stamping command does not quote its paths.
    # TEMP can contain spaces (for example a Windows account's display name).
    $projectFile = Join-Path $temporary 'UnionProject/UnionProject.vcxproj'
    $projectXml = New-Object System.Xml.XmlDocument
    $projectXml.PreserveWhitespace = $true
    $projectXml.Load($projectFile)
    $namespaces = New-Object System.Xml.XmlNamespaceManager($projectXml.NameTable)
    $namespaces.AddNamespace('msbuild', $projectXml.DocumentElement.NamespaceURI)
    $commands = $projectXml.SelectNodes('//msbuild:PreBuildEvent/msbuild:Command', $namespaces)
    $patchedCommands = 0
    foreach ($command in $commands) {
        if ($command.InnerText.Trim() -eq '$(ProjectDir)\incver.exe $(ProjectDir)') {
            # ProjectDir ends in a backslash. Double that final backslash
            # before the closing quote so the Windows CRT receives it intact.
            $command.InnerText = '"$(ProjectDir)incver.exe" "$(ProjectDir)\"'
            $patchedCommands++
        }
    }
    if ($patchedCommands -eq 0) { throw 'Pinned SDK version-stamping command was not found.' }
    $projectXml.Save($projectFile)
    $env:CL = "$previousCL /std:c++17"
    & $msbuildPath $projectFile /m '/p:Configuration=G1 Release' /p:Platform=Win32 /v:minimal
    if ($LASTEXITCODE -ne 0) { throw 'Native Union build failed.' }
    $dll = Join-Path $temporary 'UnionProject/G1 Release/UnionProject.dll'
    if (-not (Test-Path $dll)) { throw 'MSBuild did not produce the G1 DLL.' }
    $binaryText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($dll))
    if (-not $binaryText.Contains('PH_BUILD_013_TEN_SLOTS_PROJECTILE_INPUT_CAST_ANIM')) { throw 'Built DLL lacks the current plugin marker.' }
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    $destination = Join-Path $OutputDirectory 'ParryHotterHUD-G1.dll'
    Copy-Item $dll $destination -Force
    Write-Host "Built local plugin: $destination"
    Write-Host 'Reparse Scripts and rebuild the local .mod before testing in Gothic.'
}
finally {
    $env:CL = $previousCL
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
