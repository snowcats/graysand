@echo off
for %%I in ("%~dp0..") do set "UI=%%~fI"
if not exist "%UI%\build\release\ECHOV460.IMG" (
    echo Build the release image first: build\release\ECHOV460.IMG is missing
    exit /b 2
)
"%UI%\.venv\Scripts\python.exe" "%UI%\tools\echo_live.py" %*
