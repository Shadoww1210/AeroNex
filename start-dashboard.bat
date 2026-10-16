@echo off
setlocal
title PosturePal Laptop Dashboard
cd /d "%~dp0"
echo.
echo ==========================================
echo   PosturePal laptop dashboard
echo ==========================================
echo.

for %%F in (server.ps1 index.html style.css script.js team.jpeg robotics-club-logo.jpeg) do (
  if not exist "%~dp0%%F" (
    echo Missing required file: %%F
    echo Extract the complete PosturePal ZIP first, then run this launcher again.
    echo.
    pause
    exit /b 1
  )
)

echo Starting the local dashboard helper.
echo Keep this window open while using PosturePal.
echo Your browser should open at http://127.0.0.1:8787/
echo If it does not, copy that address into Chrome or Edge.
echo.

"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0server.ps1"
set "DASHBOARD_EXIT=%ERRORLEVEL%"
echo.
if not "%DASHBOARD_EXIT%"=="0" echo The dashboard stopped with error code %DASHBOARD_EXIT%.
echo The dashboard helper has stopped. Press any key to close this window.
pause
exit /b %DASHBOARD_EXIT%
