# Generator_Version.ps1
# 此脚本在CMake配置时生成version.cpp文件
# 用法: .\Generator_Version.ps1 -OutputPath <path> -BuildType <Debug|Release> -Platform <platform> -VcpkgTriplet <triplet> [-Features <f1;f2>] [-NoDefaultFeatures ON|OFF]

param(
    [Parameter(Mandatory=$true)]
    [string]$OutputPath,
    
    [Parameter(Mandatory=$true)]
    [string]$BuildType,
    
    [Parameter(Mandatory=$true)]
    [string]$Platform,
    
    [Parameter(Mandatory=$false)]
    [string]$VcpkgTriplet = "unknown",
    
    [Parameter(Mandatory=$false)]
    [string]$ConfigureTime = (Get-Date -Format "yyyy-MM-dd HH:mm:ss"),
    
    [Parameter(Mandatory=$false)]
    [string]$VcpkgJsonPath,
    
    # CMake 传入的 vcpkg manifest features（对应工具链变量 VCPKG_MANIFEST_FEATURES，
    # 分号/逗号分隔）；与 default-features 判定共同决定 thirdLibraries 与许可证
    [Parameter(Mandatory=$false)]
    [string]$Features = "",
    
    # 对应 VCPKG_MANIFEST_NO_DEFAULT_FEATURES：ON 时忽略 vcpkg.json 的 default-features
    [Parameter(Mandatory=$false)]
    [string]$NoDefaultFeatures = "OFF"
)

# 获取当前时间戳
$timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"

# 根据 vcpkg triplet 判断目标平台
function Get-PlatformFromTriplet {
    param([string]$triplet)
    
    # 游戏主机 triplet 常含 windows/linux 字样（如 xbox-scarlet-windows），需优先判定
    if ($triplet -match "xbox" -or $triplet -match "switch" -or $triplet -match "playstation" -or $triplet -match "stadia") {
        return "GameConsole"
    }
    elseif ($triplet -match "android") {
        return "Android"
    }
    elseif ($triplet -match "ios") {
        return "iOS"
    }
    elseif ($triplet -match "osx" -or $triplet -match "darwin") {
        return "macOS"
    }
    elseif ($triplet -match "linux") {
        return "Linux"
    }
    elseif ($triplet -match "windows" -or $triplet -match "mingw") {
        return "Windows"
    }
    else {
        return "Unknown"
    }
}

# 求值 vcpkg.json 的 platform 表达式：支持 |(或)、&(与)、!(非) 及平台标识，
# 如 "windows | linux | osx"、"!uwp"、"!windows & !linux & !osx & !android"
function Test-VcpkgPlatform {
    param([string]$expression, [string]$platform)
    if (-not $expression) { return $true }
    foreach ($orGroup in ($expression -split "\|")) {
        $groupResult = $true
        foreach ($term in ($orGroup -split "&")) {
            $term = $term.Trim()
            if ($term -eq "") { continue }
            if ($term.StartsWith("!")) {
                if ($term.Substring(1).Trim() -eq $platform) { $groupResult = $false; break }
            }
            elseif ($term -ne $platform) { $groupResult = $false; break }
        }
        if ($groupResult) { return $true }
    }
    return $false
}

# 获取构建所在平台(运行脚本的机器)
$buildPlatform = if ($IsWindows) {
    "Windows"
}
elseif ($IsLinux) {
    "Linux"
}
elseif ($IsMacOS) {
    "macOS"
}
else {
    # 旧版 PowerShell 或非 Core 版本
    switch ($env:OS) {
        "Windows_NT" { "Windows" }
        default { 
            if ($PSVersionTable.Platform -eq "Unix") {
                if (Get-Command uname -ErrorAction SilentlyContinue) {
                    $osName = uname -s
                    if ($osName -eq "Darwin") { "macOS" }
                    else { "Linux" }
                }
                else { "Linux" }
            }
            else { "Unknown" }
        }
    }
}

# 根据 triplet 判断目标平台
$targetPlatform = Get-PlatformFromTriplet $VcpkgTriplet

# 解析 vcpkg.json 文件
$vcpkgJsonPath = if ($VcpkgJsonPath) { $VcpkgJsonPath } else { Join-Path $PSScriptRoot "..\vcpkg.json" }
$thirdLibsCode = ""
# 是否包含 GPL/LGPL 等 Copyleft 依赖，用于决定项目许可证（GPL 或 MIT）
$hasCopyleft = $false
# 当前构建启用的 manifest features（显式传入 + 平台匹配的 default-features）
$enabledFeatures = @()

if (Test-Path $vcpkgJsonPath) {
    try {
        $vcpkgContent = Get-Content $vcpkgJsonPath -Raw -Encoding UTF8 | ConvertFrom-Json
        
        # 将目标平台转换为 vcpkg.json 中使用的平台标识
        $platformMap = @{
            "Windows" = "windows"
            "Linux" = "linux"
            "macOS" = "osx"
            "Android" = "android"
            "iOS" = "ios"
            # 游戏主机不属于 vcpkg.json 中任何 platform 列表，用独立标识以排除 CGAL/OCCT 等平台受限依赖
            "GameConsole" = "gameconsole"
        }
        $currentPlatform = $platformMap[$targetPlatform]
        
        # 定义常见库的许可证和主页信息（手动维护）
        $libInfoMap = @{
            "boost-locale" = @{ license = "BSL-1.0"; homepage = "https://www.boost.org/" }
            "boost-log" = @{ license = "BSL-1.0"; homepage = "https://www.boost.org/" }
            "boost-dll" = @{ license = "BSL-1.0"; homepage = "https://www.boost.org/" }
            "boost-multiprecision" = @{ license = "BSL-1.0"; homepage = "https://www.boost.org/" }
            "boost-nowide" = @{ license = "BSL-1.0"; homepage = "https://www.boost.org/" }
            "boost-date-time" = @{ license = "BSL-1.0"; homepage = "https://www.boost.org/" }
            "boost-property-tree" = @{ license = "BSL-1.0"; homepage = "https://www.boost.org/" }
            "boost-test" = @{ license = "BSL-1.0"; homepage = "https://www.boost.org/" }
            "boost-units" = @{ license = "BSL-1.0"; homepage = "https://www.boost.org/" }
            "boost-uuid" = @{ license = "BSL-1.0"; homepage = "https://www.boost.org/" }
            "boost-pfr" = @{ license = "BSL-1.0"; homepage = "https://www.boost.org/" }
            "cgal" = @{ license = "GPL-3.0-or-later"; homepage = "https://www.cgal.org/" }
            "clipper2" = @{ license = "BSL-1.0"; homepage = "https://github.com/AngusJohnson/Clipper2" }
            "cpp-httplib" = @{ license = "MIT"; homepage = "https://github.com/yhirose/cpp-httplib" }
            "libjpeg-turbo" = @{ license = "IJG"; homepage = "https://libjpeg-turbo.org/" }
            "libpng" = @{ license = "libpng-2.0"; homepage = "http://www.libpng.org/" }
            "opencv" = @{ license = "Apache-2.0"; homepage = "https://opencv.org/" }
            "eigen3" = @{ license = "MPL-2.0"; homepage = "https://eigen.tuxfamily.org/" }
            "libigl" = @{ license = "MIT"; homepage = "https://libigl.github.io/" }
            "freetype" = @{ license = "FTL OR GPL-2.0-or-later"; homepage = "https://freetype.org/" }
            "miniz" = @{ license = "MIT"; homepage = "https://github.com/richgel999/miniz" }
            "opencascade" = @{ license = "LGPL-2.1-only"; homepage = "https://dev.opencascade.org/" }
            "protobuf" = @{ license = "BSD-3-Clause"; homepage = "https://developers.google.com/protocol-buffers" }
            "rapidjson" = @{ license = "MIT"; homepage = "https://rapidjson.org/" }
            "tinyxml2" = @{ license = "Zlib"; homepage = "https://github.com/leethomason/tinyxml2" }
            "yaml-cpp" = @{ license = "MIT"; homepage = "https://github.com/jbeder/yaml-cpp" }
            "lua" = @{ license = "MIT"; homepage = "https://www.lua.org/" }
            "openssl" = @{ license = "Apache-2.0"; homepage = "https://www.openssl.org/" }
            "sqlpp11" = @{ license = "BSD-2-Clause"; homepage = "https://github.com/rbock/sqlpp11" }
            "magic-enum" = @{ license = "MIT"; homepage = "https://github.com/Neargye/magic_enum" }
            "bit7z" = @{ license = "MPL-2.0"; homepage = "https://github.com/mcmilk/bit7z" }
            "fontconfig" = @{ license = "MIT"; homepage = "https://fontconfig.org/" }
            "openvdb" = @{ license = "Apache-2.0"; homepage = "https://www.openvdb.org/" }
        }
        
        # default-features：按平台表达式判定默认启用的 feature（NoDefaultFeatures=ON 时忽略）
        $noDefault = $NoDefaultFeatures -in @("ON", "1", "True", "TRUE", "true")
        if (-not $noDefault -and $vcpkgContent.'default-features') {
            foreach ($df in @($vcpkgContent.'default-features')) {
                $dfName = if ($df -is [string]) { $df } else { $df.name }
                $dfPlatform = if ($df -is [object] -and $df.platform) { $df.platform } else { $null }
                if ($dfName -and (-not $dfPlatform -or (Test-VcpkgPlatform $dfPlatform $currentPlatform))) {
                    $enabledFeatures += @($dfName)
                }
            }
        }
        # 显式传入的 features（rest-api、copyleft 等，分号/逗号分隔）
        $enabledFeatures += @($Features -split "[;,]" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
        $enabledFeatures = @($enabledFeatures | Select-Object -Unique)

        # 顶层依赖 + 所有启用 feature 的依赖（copyleft→CGAL/OCCT，rest-api→cpp-httplib…）
        $allDeps = @($vcpkgContent.dependencies)
        foreach ($featName in $enabledFeatures) {
            $feat = $vcpkgContent.features.$featName
            if ($feat -and $feat.dependencies) {
                $allDeps += @($feat.dependencies)
            }
        }
        
        # 遍历依赖并生成代码
        $addedLibs = @{}
        foreach ($dep in $allDeps) {
            # 处理字符串或对象类型的依赖
            $depName = if ($dep -is [string]) { $dep } else { $dep.name }
            $depPlatform = if ($dep -is [object] -and $dep.platform) { $dep.platform } else { $null }
            
            # 检查平台条件（完整表达式求值：|/!/& 组合）
            $shouldInclude = $true
            if ($depPlatform -and $currentPlatform) {
                $shouldInclude = Test-VcpkgPlatform $depPlatform $currentPlatform
            }
            
            if ($shouldInclude -and $libInfoMap.ContainsKey($depName) -and -not $addedLibs.ContainsKey($depName)) {
                $info = $libInfoMap[$depName]
                # 许可证含 GPL/LGPL 且非 "X OR GPL" 双许可时，视为 Copyleft 依赖
                if ($info.license -match "GPL" -and $info.license -notmatch "\sOR\s") {
                    $hasCopyleft = $true
                }
                $thirdLibsCode += "    info.thirdLibraries.push_back({`"$depName`", `"$($info.license)`", `"$($info.homepage)`"});`
"
                $addedLibs[$depName] = $true
            }
        }
    }
    catch {
        Write-Warning "Failed to parse vcpkg.json: $_"
        $thirdLibsCode = "    // Failed to parse vcpkg.json, please add libraries manually`
"
    }
}
else {
    Write-Warning "vcpkg.json not found at: $vcpkgJsonPath"
    $thirdLibsCode = "    // vcpkg.json not found, please add libraries manually`
"
}

# 根据是否包含 GPL/LGPL 依赖确定项目许可证：未使用 Copyleft 依赖为 MIT，否则为 GPL
$projectLicense = if ($hasCopyleft) { "GPL-3.0-or-later" } else { "MIT" }

# 生成version.cpp内容
$versionCppContent = @"
#include "version.hpp"

namespace HsBa::Slicer::Version
{
VersionInfo GetVersionInfo()
{
    VersionInfo info;
    info.librariesName = "HsBaSlicer";
    info.license = "$projectLicense";
    info.version = "1.0.0";
    info.buildType = "$BuildType";
    info.buildPlatform = "$buildPlatform";
    info.configureTime = "$timestamp";
    info.vcpkgTargetTriplet = "$VcpkgTriplet";
    
$thirdLibsCode
    return info;
}
}  // namespace HsBa::Slicer::Version
"@

# 确保输出目录存在
$outputDir = Split-Path $OutputPath -Parent
if ($outputDir -and $outputDir -ne '' -and -not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
}

# 写入文件
# 使用UTF8无BOM编码
$utf8NoBom = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText($OutputPath, $versionCppContent, $utf8NoBom)

Write-Host "Generated version.cpp at: $OutputPath"
Write-Host "Build Type: $BuildType"
Write-Host "Platform: $Platform"
Write-Host "Vcpkg Triplet: $VcpkgTriplet"
Write-Host "Manifest Features: ${Features}"
Write-Host "Enabled Features: $($enabledFeatures -join ', ')"
Write-Host "Project License: $projectLicense"
Write-Host "Configure Time: $timestamp"
