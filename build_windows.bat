@echo off
REM ============================================================
REM  TPD - build Windows (MinGW direct, secours sans MSYS2)
REM  Produit dist\ : tdp.exe + portaudio.dll + README.md
REM
REM  Pre-requis : gcc (MinGW) et PortAudio (headers + lib) dans PATH.
REM  Sous MSYS2, "make windows" / "make dist" est preferable.
REM ============================================================
setlocal enabledelayedexpansion

set OUT=dist
if not exist %OUT% mkdir %OUT%

echo [1/3] Compilation de tdp.exe ...
gcc -std=c11 -O2 -Wall -Wextra -Iinclude ^
    -o %OUT%\tdp.exe ^
    src\main.c src\ui_windows.c src\audio.c src\dsp.c src\params.c ^
    -lportaudio -lm -static-libgcc
if errorlevel 1 (
    echo ERREUR: compilation echouee. Verifiez que gcc et PortAudio sont installes.
    exit /b 1
)

echo [2/3] Copie de portaudio.dll ...
set FOUND=0
for %%D in (
    "%~dp0mingw64\bin"
    "%~dp0mingw32\bin"
    "%PATH%"
) do (
    if exist "%%~D\libportaudio-0.dll" (
        copy /y "%%~D\libportaudio-0.dll" "%OUT%\portaudio.dll" >nul
        set FOUND=1
        goto :dldone
    )
    if exist "%%~D\portaudio.dll" (
        copy /y "%%~D\portaudio.dll" "%OUT%\portaudio.dll" >nul
        set FOUND=1
        goto :dldone
    )
)
:dldone
if "%FOUND%"=="0" (
    echo ATTENTION: portaudio.dll introuvable.
    echo   Copie-la manuellement a cote de tdp.exe (dist\portaudio.dll), sinon tdp.exe ne demarrera pas.
)

echo [3/3] Copie du README ...
copy /y README.md %OUT%\README.md >nul

echo.
echo OK : %OUT%\ contient tdp.exe (+ portaudio.dll) + README.md.
echo Lancer :  %OUT%\tdp.exe
endlocal
