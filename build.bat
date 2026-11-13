@echo off
REM ------------------------------------------------------------
REM  Weather Forecasting System - Windows build script (GCC/MinGW)
REM    build.bat        build bin\weather.exe
REM    build.bat run    build and run
REM    build.bat test   build and run the unit tests
REM ------------------------------------------------------------
setlocal enabledelayedexpansion
if not exist bin mkdir bin

set SRC=
set LIB=
for %%f in (src\*.c) do (
    set SRC=!SRC! %%f
    if /I not "%%~nf"=="main" if /I not "%%~nf"=="user_menu" if /I not "%%~nf"=="admin_menu" set LIB=!LIB! %%f
)

if /I "%1"=="test" goto test

echo Building bin\weather.exe ...
gcc -std=c99 -Wall -Wextra -O2 -Iinclude %SRC% -o bin\weather.exe -lm
if errorlevel 1 goto failed
echo Build successful: bin\weather.exe
if /I "%1"=="run" bin\weather.exe
goto end

:test
echo Building bin\run_tests.exe ...
gcc -std=c99 -Wall -Wextra -O2 -Iinclude tests\test_main.c %LIB% -o bin\run_tests.exe -lm
if errorlevel 1 goto failed
bin\run_tests.exe
goto end

:failed
echo.
echo Build FAILED. Is GCC (MinGW) installed and in PATH?
exit /b 1

:end
endlocal
