@echo off
setlocal EnableDelayedExpansion
for %%I in ("%~dp0..") do set "UI=%%~fI"
if not defined MSYS2 set "MSYS2=C:\msys64"
if not exist "%MSYS2%\usr\bin\bash.exe" (
    echo The live simulator needs MSYS2 with the CLANG64 toolchain: https://www.msys2.org
    echo Set MSYS2 to its folder if it is not in C:\msys64
    exit /b 2
)
where py >nul 2>nul && set "PYTHON=py -3"
if not defined PYTHON where python >nul 2>nul && set "PYTHON=python"
if not defined PYTHON (
    echo The live simulator needs Python 3: https://www.python.org/downloads/
    exit /b 2
)

set "THEMES=Podium Pokemon Zelda Lain Noah"
set "PICK=%~1"
if "%PICK%"=="" (
    echo Themes: 1 Podium   2 Pokemon   3 Zelda   4 Lain   5 Noah
    set /p "PICK=Pick a theme [1-5, Enter for Podium]: "
    if "!PICK!"=="" set "PICK=1"
)
set "INDEX="
set "NAME="
set /a "N=0"
for %%T in (%THEMES%) do (
    set /a "N+=1"
    if /i "%%T"=="!PICK!" set "INDEX=!N!"
    if "!N!"=="!PICK!" set "INDEX=!N!"
)
if not defined INDEX (
    echo Unknown theme "%PICK%". Use 1 to 5 or one of: %THEMES%
    exit /b 2
)
set /a "N=0"
for %%T in (%THEMES%) do (
    set /a "N+=1"
    if "!N!"=="!INDEX!" set "NAME=%%T"
)
set /a "THEME=INDEX-1"

set "MUSIC=%UI%\..\sound\music"
if not "%~2"=="" set "MUSIC=%~f2"
if not exist "%MUSIC%\" (
    echo No music folder at "%MUSIC%"
    echo Pass one as the second argument: live_sim.bat %NAME% D:\Music
    exit /b 2
)

set "SYSTEM=%UI%\build\live\%NAME%"
if /i "%~3"=="fresh" if exist "%SYSTEM%" rmdir /s /q "%SYSTEM%"
if not exist "%SYSTEM%" mkdir "%SYSTEM%"
set "SCANNED="
if not exist "%SYSTEM%\library.db" (
    for %%T in (%THEMES%) do (
        if not defined SCANNED if exist "%UI%\build\live\%%T\library.db" (
            set "SCANNED=%%T"
            for %%F in ("%UI%\build\live\%%T\*.bin" "%UI%\build\live\%%T\library.db" "%UI%\build\live\%%T\build.tmp") do (
                if /i not "%%~nxF"=="settings.bin" copy /y "%%~fF" "%SYSTEM%\" >nul
            )
        )
    )
)
if not exist "%SYSTEM%\library.db" (
    echo First run: Podium scans your whole library and finds every cover.
    echo That takes about a minute, and Windows may call the window "Not Responding" until it is done.
)
if not exist "%SYSTEM%\settings.bin" (
    %PYTHON% -c "import sys, struct; h = bytearray.fromhex('454d53540500070200000000000c02001e000000'); h[17] = int(sys.argv[2]); v = 2166136261; exec('for b in h: v = ((v ^ b) * 16777619) & 0xFFFFFFFF'); open(sys.argv[1], 'wb').write(bytes(h) + struct.pack('<I', v))" "%SYSTEM%\settings.bin" %THEME%
    if errorlevel 1 exit /b 1
)

echo Building the simulator...
pushd "%UI%"
"%MSYS2%\usr\bin\env" MSYSTEM=CLANG64 CHERE_INVOKING=1 CMAKE_GENERATOR=Ninja CC=clang "%MSYS2%\usr\bin\bash" -lc "(test -f build/dev/build.ninja || cmake --preset dev) && cmake --build build/dev --target echo_sim"
set "BUILT=%errorlevel%"
popd
if not "%BUILT%"=="0" (
    echo The simulator did not build.
    exit /b 1
)

echo.
echo Podium live: %NAME% theme, music from %MUSIC%
echo   Up / Down     volume rocker, also moves through lists
echo   Enter, Space  play, select, hold to favorite
echo   Tab, M        menu
echo   Backspace     back
echo   P             power, screen off and on
echo Settings and favorites for this theme are kept in %SYSTEM%
echo Your music folder is only read, never changed. Close the window to stop.
echo.
set "PATH=%MSYS2%\clang64\bin;%PATH%"
"%UI%\build\dev\platform\sim\echo_sim.exe" --scale 3 --card "%MUSIC%" --system-dir "%SYSTEM%"
exit /b %errorlevel%
