@echo off
setlocal
cd /d "%~dp0"

if not exist "mtp_traveler.exe" (
    echo [ERROR] mtp_traveler.exe not found in this directory.
    echo Build it first with MSYS2, or download a prebuilt binary.
    pause
    exit /b 1
)

:menu
cls
echo ==============================
echo      MTP Path Traveler
echo ==============================
echo.
echo   [1] List devices
echo   [2] List storage volumes
echo   [3] List folder contents
echo   [4] Upload a file
echo   [5] Exit
echo.
set /p choice=  Select option: 

if "%choice%"=="1" (
    mtp_traveler.exe list-devices
    pause
    goto menu
)
if "%choice%"=="2" (
    mtp_traveler.exe list-storage
    pause
    goto menu
)
if "%choice%"=="3" (
    set /p sid=  Storage ID: 
    set /p pid=  Parent ID : 
    mtp_traveler.exe list-folder %sid% %pid%
    pause
    goto menu
)
if "%choice%"=="4" (
    set /p local=  Local file : 
    set /p pid=    Parent ID  : 
    set /p target=  Target path: 
    mtp_traveler.exe put "%local%" %pid% "%target%"
    pause
    goto menu
)
if "%choice%"=="5" exit /b 0

goto menu