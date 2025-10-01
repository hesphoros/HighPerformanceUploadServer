# Build-WithTOML.ps1
# 构建带TOML支持的上传客户端

param(
    [string]$BuildType = "Release",
    [switch]$Clean,
    [switch]$InstallTOML,
    [switch]$ValidateConfig,
    [switch]$Help
)

if ($Help) {
    Write-Host "=== 高性能上传客户端TOML构建脚本 ===" -ForegroundColor Green
    Write-Host ""
    Write-Host "用法:"
    Write-Host "  .\Build-WithTOML.ps1 [选项]"
    Write-Host ""
    Write-Host "选项:"
    Write-Host "  -BuildType <Debug|Release>  构建类型 (默认: Release)"
    Write-Host "  -Clean                      清理构建目录"
    Write-Host "  -InstallTOML               安装TOML库依赖"
    Write-Host "  -ValidateConfig            验证TOML配置文件"
    Write-Host "  -Help                      显示此帮助信息"
    Write-Host ""
    Write-Host "示例:"
    Write-Host "  .\Build-WithTOML.ps1 -InstallTOML          # 安装TOML依赖"
    Write-Host "  .\Build-WithTOML.ps1 -Clean -BuildType Debug  # 清理并调试构建"
    Write-Host "  .\Build-WithTOML.ps1 -ValidateConfig       # 验证配置文件"
    exit 0
}

# 设置错误处理
$ErrorActionPreference = "Stop"

# 获取脚本目录
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ClientDir = Split-Path -Parent $ScriptDir

Write-Host "=== 高性能上传客户端TOML构建 ===" -ForegroundColor Green
Write-Host "客户端目录: $ClientDir" -ForegroundColor Cyan
Write-Host "构建类型: $BuildType" -ForegroundColor Cyan

# 检查必要工具
function Test-Command($Command) {
    try {
        Get-Command $Command -ErrorAction Stop | Out-Null
        return $true
    } catch {
        return $false
    }
}

Write-Host "`n🔍 检查构建工具..." -ForegroundColor Yellow

if (-not (Test-Command "cmake")) {
    Write-Error "CMake 未找到，请先安装 CMake"
}

if (-not (Test-Command "vcpkg")) {
    Write-Warning "vcpkg 未找到，将使用FetchContent下载TOML库"
}

# 安装TOML库
if ($InstallTOML) {
    Write-Host "`n📦 安装TOML库依赖..." -ForegroundColor Yellow
    
    if (Test-Command "vcpkg") {
        try {
            Write-Host "使用vcpkg安装toml11..." -ForegroundColor Cyan
            & vcpkg install toml11:x64-windows
            
            if ($LASTEXITCODE -eq 0) {
                Write-Host "✅ toml11 安装成功" -ForegroundColor Green
            } else {
                Write-Warning "vcpkg 安装失败，构建时将使用FetchContent"
            }
        } catch {
            Write-Warning "vcpkg 安装出错: $($_.Exception.Message)"
        }
    }
    
    # 安装Python TOML验证工具依赖
    if (Test-Command "python") {
        try {
            Write-Host "安装Python TOML验证工具依赖..." -ForegroundColor Cyan
            & python -m pip install tomli
            Write-Host "✅ Python TOML工具依赖安装完成" -ForegroundColor Green
        } catch {
            Write-Warning "Python TOML依赖安装失败: $($_.Exception.Message)"
        }
    }
}

# 验证配置文件
if ($ValidateConfig) {
    Write-Host "`n🔧 验证TOML配置文件..." -ForegroundColor Yellow
    
    $ConfigFiles = @(
        "config\upload_client.toml",
        "config\upload_client_complete.toml"
    )
    
    foreach ($ConfigFile in $ConfigFiles) {
        $ConfigPath = Join-Path $ClientDir $ConfigFile
        if (Test-Path $ConfigPath) {
            Write-Host "验证: $ConfigFile" -ForegroundColor Cyan
            
            if (Test-Command "python") {
                $ValidatorScript = Join-Path $ClientDir "tools\toml_validator.py"
                if (Test-Path $ValidatorScript) {
                    try {
                        & python $ValidatorScript validate $ConfigPath
                    } catch {
                        Write-Warning "配置验证失败: $($_.Exception.Message)"
                    }
                } else {
                    Write-Warning "验证工具不存在: $ValidatorScript"
                }
            } else {
                Write-Warning "Python未安装，跳过配置验证"
            }
        } else {
            Write-Warning "配置文件不存在: $ConfigFile"
        }
    }
}

# 清理构建目录
if ($Clean) {
    Write-Host "`n🧹 清理构建目录..." -ForegroundColor Yellow
    $BuildDir = Join-Path $ClientDir "build"
    if (Test-Path $BuildDir) {
        Remove-Item $BuildDir -Recurse -Force
        Write-Host "✅ 构建目录已清理" -ForegroundColor Green
    }
}

# 创建构建目录
$BuildDir = Join-Path $ClientDir "build"
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
    Write-Host "📁 创建构建目录: $BuildDir" -ForegroundColor Cyan
}

# 进入构建目录
Set-Location $BuildDir

Write-Host "`n🔨 开始CMake配置..." -ForegroundColor Yellow

# CMake配置
try {
    $CMakeArgs = @(
        ".."
        "-DCMAKE_BUILD_TYPE=$BuildType"
        "-DCMAKE_PREFIX_PATH=D:/cppsoft/vcpkg/installed/x64-windows"
        "-DCMAKE_TOOLCHAIN_FILE=D:/cppsoft/vcpkg/scripts/buildsystems/vcpkg.cmake"
    )
    
    & cmake @CMakeArgs
    
    if ($LASTEXITCODE -ne 0) {
        throw "CMake配置失败"
    }
    
    Write-Host "✅ CMake配置完成" -ForegroundColor Green
} catch {
    Write-Error "CMake配置失败: $($_.Exception.Message)"
}

Write-Host "`n🔨 开始编译..." -ForegroundColor Yellow

# 编译项目
try {
    & cmake --build . --config $BuildType --parallel
    
    if ($LASTEXITCODE -ne 0) {
        throw "编译失败"
    }
    
    Write-Host "✅ 编译完成" -ForegroundColor Green
} catch {
    Write-Error "编译失败: $($_.Exception.Message)"
}

# 检查生成的可执行文件
$ExeFile = Join-Path $BuildDir "bin\UploadClient.exe"
if (Test-Path $ExeFile) {
    Write-Host "✅ 可执行文件生成成功: $ExeFile" -ForegroundColor Green
    
    # 显示文件信息
    $FileInfo = Get-Item $ExeFile
    Write-Host "   大小: $([math]::Round($FileInfo.Length / 1MB, 2)) MB" -ForegroundColor Cyan
    Write-Host "   修改时间: $($FileInfo.LastWriteTime)" -ForegroundColor Cyan
} else {
    Write-Warning "可执行文件未找到: $ExeFile"
}

Write-Host "`n🎉 构建完成!" -ForegroundColor Green
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray

Write-Host "`n💡 下一步建议:" -ForegroundColor Yellow
Write-Host "1. 复制配置文件到可执行文件目录："
Write-Host "   copy config\upload_client.toml build\bin\"
Write-Host "2. 运行客户端："
Write-Host "   .\build\bin\UploadClient.exe"
Write-Host "3. 验证TOML配置功能："
Write-Host "   python tools\toml_validator.py validate config\upload_client.toml"

# 返回原目录
Set-Location $ClientDir