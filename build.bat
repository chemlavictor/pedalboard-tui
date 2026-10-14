@echo off
REM ============================================================
REM  TPD - BUILD sur Windows
REM  - Si MSYS2/MinGW gcc + PortAudio sont dans PATH : compile tdp.exe -> dist/
REM  - Sinon : message clair + instructions.
REM  Ce script ne fonctionne que sous Windows NATIF (pas WSL, pas \\wsl.localhost).
REM ============================================================
setlocal
set "ROOT=%~dp0"
cd /d "%ROOT%" || (echo Impossible de changer de repertoire. & exit /b 1)

where gcc >nul 2>nul
if errorlevel 1 (
    echo [ERREUR] gcc (MinGW) introuvable dans le PATH.
    echo.
    echo  Pour compiler tdp.exe, installe MSYS2 puis :
    echo     1. Installe MSYS2 : https://www.msys2.org/
    echo     2. Dans le shell MSYS2 :
    echo          pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-portaudio
    echo     3. Relance ce script depuis le shell MSYS2 (ou "make dist").
    echo.
    echo  Alternative : lance build_windows.bat (MinGW direct).
    echo  Sous WSL : utilise "make" (version Linux), pas ce script.
    exit /b 1
)

echo [build] gcc detecte :
where gcc
echo.
REM Delege a build_windows.bat (compilation + copie DLL + dist/).
call "%ROOT%build_windows.bat"
endlocal
