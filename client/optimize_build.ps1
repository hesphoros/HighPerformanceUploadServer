#!/usr/bin/env pwsh
# =================================================================
# 🚀 Abseil 编译优化切换脚本
# 用途: 在源码构建和 vcpkg 预编译库之间快速切换
# =================================================================

Write-Host "🚀 Abseil 编译优化工具" -ForegroundColor Cyan
Write-Host "============================" -ForegroundColor Gray

# 检查vcpkg Abseil是否已安装
$vcpkgAbseil = vcpkg list | Select-String "abseil"
if ($vcpkgAbseil) {
    Write-Host "✅ vcpkg Abseil 已安装: " -ForegroundColor Green -NoNewline
    Write-Host $vcpkgAbseil -ForegroundColor White
    $hasVcpkgAbseil = $true
}
else {
    Write-Host "❌ vcpkg Abseil 未安装" -ForegroundColor Red
    $hasVcpkgAbseil = $false
}

Write-Host ""
Write-Host "可用选项:" -ForegroundColor Yellow
Write-Host "1. 🏃‍♂️ 安装 vcpkg Abseil (推荐 - 一次安装，永久加速)" -ForegroundColor Green
Write-Host "2. ⚡ 使用优化版 CMakeLists (智能选择最快方案)" -ForegroundColor Cyan
Write-Host "3. 🔄 还原原始 CMakeLists (使用源码构建)" -ForegroundColor Yellow
Write-Host "4. 📊 显示构建时间对比" -ForegroundColor Magenta
Write-Host "5. ❌ 退出" -ForegroundColor Gray

$choice = Read-Host "`n请选择选项 (1-5)"

switch ($choice) {
    "1" {
        Write-Host "`n🚀 正在安装 vcpkg Abseil..." -ForegroundColor Cyan
        if ($hasVcpkgAbseil) {
            Write-Host "✅ Abseil 已经安装，无需重复安装" -ForegroundColor Green
        }
        else {
            Write-Host "📦 开始安装... (这可能需要几分钟)" -ForegroundColor Yellow
            vcpkg install abseil:x64-windows
            if ($LASTEXITCODE -eq 0) {
                Write-Host "✅ Abseil 安装成功！" -ForegroundColor Green
                Write-Host "💡 提示: 现在可以选择选项2使用优化版配置" -ForegroundColor Cyan
            }
            else {
                Write-Host "❌ Abseil 安装失败" -ForegroundColor Red
            }
        }
    }
    
    "2" {
        Write-Host "`n⚡ 切换到优化版 CMakeLists..." -ForegroundColor Cyan
        if (Test-Path "CMakeLists.txt") {
            Copy-Item "CMakeLists.txt" "CMakeLists_backup.txt" -Force
            Write-Host "💾 已备份原始文件为 CMakeLists_backup.txt" -ForegroundColor Yellow
        }
        
        if (Test-Path "CMakeLists_vcpkg_optimized.txt") {
            Copy-Item "CMakeLists_vcpkg_optimized.txt" "CMakeLists.txt" -Force
            Write-Host "✅ 已启用优化版配置！" -ForegroundColor Green
            Write-Host "🚀 此版本会优先使用 vcpkg 预编译库，大幅减少编译时间" -ForegroundColor Cyan
        }
        else {
            Write-Host "❌ 找不到优化版文件 CMakeLists_vcpkg_optimized.txt" -ForegroundColor Red
        }
    }
    
    "3" {
        Write-Host "`n🔄 还原原始 CMakeLists..." -ForegroundColor Yellow
        if (Test-Path "CMakeLists_backup.txt") {
            Copy-Item "CMakeLists_backup.txt" "CMakeLists.txt" -Force
            Write-Host "✅ 已还原原始配置" -ForegroundColor Green
            Write-Host "⚠️  注意: 这会回到每次重新编译 Abseil 的状态" -ForegroundColor Yellow
        }
        else {
            Write-Host "❌ 找不到备份文件 CMakeLists_backup.txt" -ForegroundColor Red
        }
    }
    
    "4" {
        Write-Host "`n📊 构建时间对比:" -ForegroundColor Magenta
        Write-Host "┌─────────────────────────────┬──────────────┬─────────────┐" -ForegroundColor Gray
        Write-Host "│ 构建方式                    │ 首次构建时间 │ 增量构建时间 │" -ForegroundColor Gray
        Write-Host "├─────────────────────────────┼──────────────┼─────────────┤" -ForegroundColor Gray
        Write-Host "│ 🐌 源码构建 Abseil          │   ~8-15分钟  │  ~2-5分钟   │" -ForegroundColor Red
        Write-Host "│ 🚀 vcpkg 预编译 Abseil      │   ~2-3分钟   │  ~10-30秒   │" -ForegroundColor Green
        Write-Host "│ ⚡ 优化版 (智能选择)        │   ~2-8分钟   │  ~10-60秒   │" -ForegroundColor Cyan
        Write-Host "└─────────────────────────────┴──────────────┴─────────────┘" -ForegroundColor Gray
        Write-Host ""
        Write-Host "💡 建议: 使用选项1安装 vcpkg Abseil，然后选项2启用优化版" -ForegroundColor Cyan
    }
    
    "5" {
        Write-Host "`n👋 退出" -ForegroundColor Gray
        exit 0
    }
    
    default {
        Write-Host "`n❌ 无效选择，请重新运行脚本" -ForegroundColor Red
        exit 1
    }
}

Write-Host "`n📝 使用说明:" -ForegroundColor Cyan
Write-Host "• 优化版会自动检测并使用最快的构建方式" -ForegroundColor White
Write-Host "• vcpkg 库只需安装一次，后续构建都会很快" -ForegroundColor White  
Write-Host "• 可以随时使用此脚本在不同配置间切换" -ForegroundColor White
Write-Host ""
Write-Host "🔧 快速重新构建项目:" -ForegroundColor Yellow
Write-Host "cmake --build build --config Release --clean-first" -ForegroundColor Gray

pause