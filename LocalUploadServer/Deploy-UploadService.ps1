# 🛡️ 高性能文件上传服务 - Windows服务部署脚本
# 
# 用途：快速部署、安装、启动、管理Windows服务
# 使用方法：以管理员权限运行PowerShell，然后执行此脚本
# 
# 支持操作：
# - 编译服务程序
# - 安装/卸载Windows服务
# - 启动/停止/重启服务
# - 查看服务状态和日志
# - 服务配置管理

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("build", "install", "uninstall", "start", "stop", "restart", "status", "logs", "config", "help")]
    [string]$Action = "help",
    
    [Parameter(Mandatory=$false)]
    [string]$ServicePath = "",
    
    [Parameter(Mandatory=$false)]
    [switch]$Force = $false
)

# 🎯 服务配置常量
$SERVICE_NAME = "HighPerformanceUploadService"
$SERVICE_DISPLAY_NAME = "高性能文件上传服务"
$SERVICE_DESCRIPTION = "提供高并发文件上传功能的后台服务，支持断点续传、进度回调、多线程并发等功能"
$SERVICE_EXE = "UploadService.exe"
$CONFIG_FILE = "config\service.json"

# 🎨 颜色输出函数
function Write-ColorOutput($ForegroundColor) {
    # 允许管道输入
    $input | ForEach-Object { Write-Host $_ -ForegroundColor $ForegroundColor }
}

function Write-Success { $input | Write-ColorOutput Green }
function Write-Warning { $input | Write-ColorOutput Yellow }
function Write-Error { $input | Write-ColorOutput Red }
function Write-Info { $input | Write-ColorOutput Cyan }

# 🔍 检查管理员权限
function Test-Administrator {
    $currentUser = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($currentUser)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# 📁 获取服务安装路径
function Get-ServicePath {
    if ($ServicePath -ne "") {
        return $ServicePath
    }
    
    # 默认使用当前目录
    $currentPath = Get-Location
    $exePath = Join-Path $currentPath $SERVICE_EXE
    
    if (Test-Path $exePath) {
        return $exePath
    }
    
    # 检查是否在源码目录
    $buildPath = Join-Path $currentPath "build\Release\$SERVICE_EXE"
    if (Test-Path $buildPath) {
        return $buildPath
    }
    
    # 检查Program Files
    $programPath = "C:\Program Files\HighPerformanceUpload\$SERVICE_EXE"
    if (Test-Path $programPath) {
        return $programPath
    }
    
    Write-Error "❌ 找不到服务可执行文件 $SERVICE_EXE"
    Write-Info "💡 请指定 -ServicePath 参数或确保可执行文件在当前目录"
    exit 1
}

# 🏗️ 编译服务程序
function Build-Service {
    Write-Info "🏗️ 编译高性能文件上传服务..."
    
    # 检查Visual Studio环境
    $vcvars = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) {
        $vcvars = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    }
    
    if (-not (Test-Path $vcvars)) {
        Write-Error "❌ 未找到Visual Studio编译环境"
        Write-Info "💡 请安装Visual Studio 2019/2022 Professional 或更高版本"
        exit 1
    }
    
    # 创建编译目录
    $buildDir = "build\Release"
    if (-not (Test-Path $buildDir)) {
        New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
    }
    
    # 编译命令
    $compileCmd = @"
call "$vcvars"
cd /d "$((Get-Location).Path)"
cl /EHsc /MT /O2 /DNDEBUG /DWIN32_LEAN_AND_MEAN ^
   src\UploadService.cpp src\LocalUploadService.cpp src\SocketServer.cpp ^
   /Fe:$buildDir\$SERVICE_EXE ^
   /link ws2_32.lib advapi32.lib shell32.lib shlwapi.lib
"@
    
    $compileCmd | Out-File -FilePath "compile.bat" -Encoding ASCII
    
    try {
        $result = & cmd /c "compile.bat"
        Remove-Item "compile.bat" -Force
        
        if (Test-Path "$buildDir\$SERVICE_EXE") {
            "✅ 编译成功！可执行文件：$buildDir\$SERVICE_EXE" | Write-Success
        } else {
            "❌ 编译失败" | Write-Error
            Write-Info $result
            exit 1
        }
    } catch {
        "❌ 编译过程中发生错误：$($_.Exception.Message)" | Write-Error
        exit 1
    }
}

# 📦 安装Windows服务
function Install-UploadService {
    if (-not (Test-Administrator)) {
        "❌ 安装服务需要管理员权限" | Write-Error
        "💡 请以管理员身份运行PowerShell" | Write-Info
        exit 1
    }
    
    $exePath = Get-ServicePath
    "🔧 安装Windows服务..." | Write-Info
    "📍 可执行文件路径：$exePath" | Write-Info
    
    try {
        # 检查服务是否已存在
        $existingService = Get-Service -Name $SERVICE_NAME -ErrorAction SilentlyContinue
        if ($existingService) {
            if ($Force) {
                "⚠️ 服务已存在，强制重新安装..." | Write-Warning
                Uninstall-UploadService
            } else {
                "❌ 服务已存在，使用 -Force 参数强制重新安装" | Write-Error
                exit 1
            }
        }
        
        # 执行安装
        $result = & $exePath install
        if ($LASTEXITCODE -eq 0) {
            "✅ Windows服务安装成功！" | Write-Success
            "📋 服务名称：$SERVICE_NAME" | Write-Info
            "📋 显示名称：$SERVICE_DISPLAY_NAME" | Write-Info
            
            # 设置服务描述和故障恢复
            sc.exe description $SERVICE_NAME $SERVICE_DESCRIPTION | Out-Null
            sc.exe failure $SERVICE_NAME reset= 86400 actions= restart/60000/restart/120000/reboot/300000 | Out-Null
            
            "💡 提示：使用 'Deploy-UploadService.ps1 start' 启动服务" | Write-Info
        } else {
            "❌ 服务安装失败" | Write-Error
            Write-Info $result
            exit 1
        }
    } catch {
        "❌ 安装过程中发生错误：$($_.Exception.Message)" | Write-Error
        exit 1
    }
}

# 🗑️ 卸载Windows服务
function Uninstall-UploadService {
    if (-not (Test-Administrator)) {
        "❌ 卸载服务需要管理员权限" | Write-Error
        exit 1
    }
    
    "🗑️ 卸载Windows服务..." | Write-Info
    
    try {
        # 检查服务是否存在
        $service = Get-Service -Name $SERVICE_NAME -ErrorAction SilentlyContinue
        if (-not $service) {
            "⚠️ 服务不存在，无需卸载" | Write-Warning
            return
        }
        
        # 停止服务
        if ($service.Status -eq "Running") {
            "⏹️ 正在停止服务..." | Write-Info
            Stop-Service -Name $SERVICE_NAME -Force
            Start-Sleep -Seconds 3
        }
        
        # 执行卸载
        $exePath = Get-ServicePath
        $result = & $exePath uninstall
        
        if ($LASTEXITCODE -eq 0) {
            "✅ Windows服务卸载成功！" | Write-Success
        } else {
            "❌ 服务卸载失败" | Write-Error
            Write-Info $result
        }
    } catch {
        "❌ 卸载过程中发生错误：$($_.Exception.Message)" | Write-Error
    }
}

# ▶️ 启动服务
function Start-UploadService {
    "▶️ 启动高性能文件上传服务..." | Write-Info
    
    try {
        $service = Get-Service -Name $SERVICE_NAME -ErrorAction SilentlyContinue
        if (-not $service) {
            "❌ 服务未安装，请先运行 'Deploy-UploadService.ps1 install'" | Write-Error
            exit 1
        }
        
        if ($service.Status -eq "Running") {
            "✅ 服务已在运行中" | Write-Success
            return
        }
        
        Start-Service -Name $SERVICE_NAME
        Start-Sleep -Seconds 2
        
        $service = Get-Service -Name $SERVICE_NAME
        if ($service.Status -eq "Running") {
            "✅ 服务启动成功！" | Write-Success
            "🌐 Socket监听端口：8901" | Write-Info
            "📊 UI回调端口：8902" | Write-Info
        } else {
            "❌ 服务启动失败，当前状态：$($service.Status)" | Write-Error
        }
    } catch {
        "❌ 启动过程中发生错误：$($_.Exception.Message)" | Write-Error
    }
}

# ⏹️ 停止服务
function Stop-UploadService {
    "⏹️ 停止高性能文件上传服务..." | Write-Info
    
    try {
        $service = Get-Service -Name $SERVICE_NAME -ErrorAction SilentlyContinue
        if (-not $service) {
            "❌ 服务未安装" | Write-Error
            return
        }
        
        if ($service.Status -eq "Stopped") {
            "✅ 服务已停止" | Write-Success
            return
        }
        
        Stop-Service -Name $SERVICE_NAME -Force
        Start-Sleep -Seconds 3
        
        $service = Get-Service -Name $SERVICE_NAME
        if ($service.Status -eq "Stopped") {
            "✅ 服务停止成功！" | Write-Success
        } else {
            "❌ 服务停止失败，当前状态：$($service.Status)" | Write-Error
        }
    } catch {
        "❌ 停止过程中发生错误：$($_.Exception.Message)" | Write-Error
    }
}

# 🔄 重启服务
function Restart-UploadService {
    "🔄 重启高性能文件上传服务..." | Write-Info
    Stop-UploadService
    Start-Sleep -Seconds 2
    Start-UploadService
}

# 📊 查看服务状态
function Show-ServiceStatus {
    "📊 高性能文件上传服务状态" | Write-Info
    "=" * 50 | Write-Info
    
    try {
        $service = Get-Service -Name $SERVICE_NAME -ErrorAction SilentlyContinue
        if (-not $service) {
            "❌ 服务未安装" | Write-Error
            return
        }
        
        "🏷️ 服务名称：$($service.Name)" | Write-Info
        "📋 显示名称：$($service.DisplayName)" | Write-Info
        "🔄 当前状态：$($service.Status)" | Write-Info
        "⚙️ 启动类型：$($service.StartType)" | Write-Info
        
        # 获取进程信息
        if ($service.Status -eq "Running") {
            $process = Get-Process -Name ($SERVICE_EXE -replace '\.exe$', '') -ErrorAction SilentlyContinue
            if ($process) {
                "🆔 进程ID：$($process.Id)" | Write-Info
                "💾 内存使用：{0:N2} MB" -f ($process.WorkingSet / 1MB) | Write-Info
                "⏰ 运行时间：$((Get-Date) - $process.StartTime)" | Write-Info
            }
        }
        
        # 检查端口监听
        "🌐 网络端口状态：" | Write-Info
        $ports = @(8901, 8902)
        foreach ($port in $ports) {
            $connection = Get-NetTCPConnection -LocalPort $port -ErrorAction SilentlyContinue
            if ($connection) {
                "   ✅ 端口 $port 正在监听" | Write-Success
            } else {
                "   ❌ 端口 $port 未监听" | Write-Error
            }
        }
        
    } catch {
        "❌ 获取状态时发生错误：$($_.Exception.Message)" | Write-Error
    }
}

# 📄 查看日志
function Show-ServiceLogs {
    "📄 查看服务日志" | Write-Info
    "=" * 50 | Write-Info
    
    # Windows事件日志
    "🔍 Windows事件日志（最近10条）：" | Write-Info
    try {
        $events = Get-WinEvent -FilterHashtable @{LogName='Application'; ProviderName=$SERVICE_NAME} -MaxEvents 10 -ErrorAction SilentlyContinue
        if ($events) {
            foreach ($event in $events) {
                $level = switch ($event.LevelDisplayName) {
                    "Information" { "INFO" }
                    "Warning" { "WARN" }
                    "Error" { "ERROR" }
                    default { $event.LevelDisplayName }
                }
                "[$($event.TimeCreated.ToString('yyyy-MM-dd HH:mm:ss'))] [$level] $($event.Message)" | Write-Info
            }
        } else {
            "   (无事件日志记录)" | Write-Info
        }
    } catch {
        "   ❌ 无法读取事件日志：$($_.Exception.Message)" | Write-Warning
    }
    
    # 文件日志
    "" | Write-Info
    "📝 文件日志（最近20行）：" | Write-Info
    $logPath = "C:\ProgramData\HighPerformanceUpload\Logs\UploadService.log"
    if (Test-Path $logPath) {
        Get-Content $logPath -Tail 20 | ForEach-Object { "   $_" | Write-Info }
    } else {
        "   (文件日志不存在：$logPath)" | Write-Info
    }
}

# ⚙️ 配置管理
function Manage-ServiceConfig {
    "⚙️ 服务配置管理" | Write-Info
    "=" * 50 | Write-Info
    
    $configPath = $CONFIG_FILE
    if (Test-Path $configPath) {
        "📁 配置文件路径：$configPath" | Write-Info
        "📄 配置文件内容：" | Write-Info
        Get-Content $configPath | ForEach-Object { "   $_" | Write-Info }
    } else {
        "❌ 配置文件不存在：$configPath" | Write-Error
        "💡 请确保配置文件存在或重新安装服务" | Write-Info
    }
}

# 📖 显示帮助
function Show-Help {
    @"
🛡️ 高性能文件上传服务 - Windows服务部署脚本

用法：
    Deploy-UploadService.ps1 [Action] [参数]

支持操作：
    build      - 编译服务程序
    install    - 安装Windows服务
    uninstall  - 卸载Windows服务
    start      - 启动服务
    stop       - 停止服务
    restart    - 重启服务
    status     - 查看服务状态
    logs       - 查看服务日志
    config     - 管理服务配置
    help       - 显示此帮助信息

参数：
    -ServicePath <路径>  - 指定服务可执行文件路径
    -Force              - 强制执行操作（用于install）

示例：
    # 编译服务程序
    .\Deploy-UploadService.ps1 build
    
    # 安装服务（需要管理员权限）
    .\Deploy-UploadService.ps1 install
    
    # 启动服务
    .\Deploy-UploadService.ps1 start
    
    # 查看服务状态
    .\Deploy-UploadService.ps1 status
    
    # 查看日志
    .\Deploy-UploadService.ps1 logs
    
    # 指定可执行文件路径安装
    .\Deploy-UploadService.ps1 install -ServicePath "C:\MyApp\UploadService.exe"

注意事项：
    - 安装/卸载服务需要管理员权限
    - 确保防火墙允许端口8901和8902的通信
    - 服务配置文件：config\service.json
    - 日志文件：C:\ProgramData\HighPerformanceUpload\Logs\

"@ | Write-Info
}

# 🎯 主程序入口
switch ($Action.ToLower()) {
    "build" { Build-Service }
    "install" { Install-UploadService }
    "uninstall" { Uninstall-UploadService }
    "start" { Start-UploadService }
    "stop" { Stop-UploadService }
    "restart" { Restart-UploadService }
    "status" { Show-ServiceStatus }
    "logs" { Show-ServiceLogs }
    "config" { Manage-ServiceConfig }
    "help" { Show-Help }
    default { Show-Help }
}
