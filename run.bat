@echo off
REM ============================================================
REM  TPD - LANCER sur Windows
REM  Fonctionne depuis n'importe quel dossier (pas de chemin UNC).
REM  Cherche tdp.exe ici, dans dist/, ou dans build/.
REM ============================================================
setlocal
set "ROOT=%~dp0"
set "EXE="

if exist "%ROOT%tdp.exe"      set "EXE=%ROOT%tdp.exe"
if not defined EXE if exist "%ROOT%dist\tdp.exe" set "EXE=%ROOT%dist\tdp.exe"
if not defined EXE if exist "%ROOT%build\tdp.exe" set "EXE=%ROOT%build\tdp.exe"

if not defined EXE (
    echo tdp.exe introuvable.
    echo   - Si tu n'as PAS compile :  lance build.bat  (ou build_windows.bat)
    echo   - Si tu es sous WSL : utilise "make run" dans WSL, pas ce script.
    exit /b 1
)

REM La DLL PortAudio doit etre a cote de l'exe.
set "DLLDIR=%~dp0%EXE%"
if not exist "%DLLDIR%portaudio.dll" if not exist "%DLLDIR%libportaudio-0.dll" (
    echo ATTENTION : portaudio.dll introuvable a cote de tdp.exe.
    echo   Copie-la dans le meme dossier que tdp.exe, sinon TPD ne demarrera pas.
)

echo Demarrage de : %EXE%
cd /d "%DLLDIR%"
"%EXE%"
endlocal
