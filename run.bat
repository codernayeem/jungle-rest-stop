@echo off
REM ==========================================================================
REM  Jungle Rest Stop - one click build + run (MinGW-w64 / MSYS2 g++)
REM ==========================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"
echo Compiling Jungle Rest Stop (this takes about a minute)...
set SRCS=
for /r src %%f in (*.cpp) do set SRCS=!SRCS! "%%f"
g++ -std=c++17 -O2 -Iinclude -Isrc !SRCS! src\glad.c -Llib -lglfw3 -lopengl32 -lgdi32 -lwinmm -static-libgcc -static-libstdc++ -o main.exe
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] Build finished. Starting main.exe ...
    "%~dp0main.exe" %*
) else (
    echo [ERROR] Build failed!
    pause
)
