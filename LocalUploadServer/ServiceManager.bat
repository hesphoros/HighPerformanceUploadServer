@echo off
:: 🛡️ 高性能文件上传服务 - 快速管理脚本
:: 
:: 用途：快速管理Windows服务的启动、停止、状态查看等操作
:: 使用方法：双击运行或在命令行中执行
:: 
:: 注意：安装/卸载服务需要以管理员权限运行

setlocal
set SERVICE_NAME=HighPerformanceUploadService
set SCRIPT_PATH=%~dp0Deploy-UploadService.ps1

:MENU
cls
echo.
echo ====================================================
echo            🛡️ 高性能文件上传服务管理
echo ====================================================
echo.
echo  请选择操作：
echo.
echo  [1] 编译服务程序
echo  [2] 安装Windows服务 (需要管理员权限)
echo  [3] 卸载Windows服务 (需要管理员权限)
echo  [4] 启动服务
echo  [5] 停止服务
echo  [6] 重启服务
echo  [7] 查看服务状态
echo  [8] 查看服务日志
echo  [9] 管理服务配置
echo  [0] 退出
echo.
echo ====================================================

set /p choice=请输入选项 (0-9): 

if "%choice%"=="1" goto BUILD
if "%choice%"=="2" goto INSTALL
if "%choice%"=="3" goto UNINSTALL
if "%choice%"=="4" goto START
if "%choice%"=="5" goto STOP
if "%choice%"=="6" goto RESTART
if "%choice%"=="7" goto STATUS
if "%choice%"=="8" goto LOGS
if "%choice%"=="9" goto CONFIG
if "%choice%"=="0" goto EXIT

echo 无效选项，请重新选择...
timeout /t 2 >nul
goto MENU

:BUILD
echo.
echo 🏗️ 编译服务程序...
powershell -ExecutionPolicy Bypass -File "%SCRIPT_PATH%" build
echo.
pause
goto MENU

:INSTALL
echo.
echo 📦 安装Windows服务...
echo 注意：此操作需要管理员权限
powershell -ExecutionPolicy Bypass -File "%SCRIPT_PATH%" install
echo.
pause
goto MENU

:UNINSTALL
echo.
echo 🗑️ 卸载Windows服务...
echo 注意：此操作需要管理员权限
powershell -ExecutionPolicy Bypass -File "%SCRIPT_PATH%" uninstall
echo.
pause
goto MENU

:START
echo.
echo ▶️ 启动服务...
powershell -ExecutionPolicy Bypass -File "%SCRIPT_PATH%" start
echo.
pause
goto MENU

:STOP
echo.
echo ⏹️ 停止服务...
powershell -ExecutionPolicy Bypass -File "%SCRIPT_PATH%" stop
echo.
pause
goto MENU

:RESTART
echo.
echo 🔄 重启服务...
powershell -ExecutionPolicy Bypass -File "%SCRIPT_PATH%" restart
echo.
pause
goto MENU

:STATUS
echo.
echo 📊 查看服务状态...
powershell -ExecutionPolicy Bypass -File "%SCRIPT_PATH%" status
echo.
pause
goto MENU

:LOGS
echo.
echo 📄 查看服务日志...
powershell -ExecutionPolicy Bypass -File "%SCRIPT_PATH%" logs
echo.
pause
goto MENU

:CONFIG
echo.
echo ⚙️ 管理服务配置...
powershell -ExecutionPolicy Bypass -File "%SCRIPT_PATH%" config
echo.
pause
goto MENU

:EXIT
echo.
echo 谢谢使用！
timeout /t 1 >nul
exit /b 0
