@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build.ps1"
set "BUILD_EXIT=%ERRORLEVEL%"
echo.
if not "%BUILD_EXIT%"=="0" (
  echo Build failed. Please send the error output for review.
) else (
  echo Build completed. Outputs:
  echo   %~dp0MouseCursorDetector.exe
  echo   %~dp0MouseCursorDetector-Setup.exe
)
pause
exit /b %BUILD_EXIT%
