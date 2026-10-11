@echo off
REM ============================================================
REM  TDP - Installation Windows
REM
REM  - Si MSYS2 + gcc sont deja presentes : compile dist\ directement.
REM  - Sinon : ouvre le shell MSYS2 MINGW64 avec les instructions
REM    (telecharger/installer MSYS2 a la main est plus fiable que
REM    le telechargement auto, dont l'URL change souvent).
REM
REM  A la fin : dist\tdp.exe + dist\portaudio.dll + dist\README.md
REM ============================================================
setlocal EnableExtensions
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "MSYS=C:\msys64"
REM Chemin du repo en format MSYS (/c/...) pour que make/bash le trouvent.
set "MROOT=%ROOT:C:\=/c/%"
set "MROOT=%MROOT:\=/%"

echo ============================================================
echo  TDP - Installation Windows
echo ============================================================
cd /d "%ROOT%" || (echo Erreur: impossible de changer de dossier. & exit /b 1)
echo [info] repo : %ROOT%

REM ---- MSYS2 + gcc presents ? -> build direct ----
if exist "%MSYS%\mingw64\bin\gcc.exe" (
    echo [1/2] gcc deja present -> compilation.
    "%MSYS%\usr\bin\bash.exe" -lc "cd '%MROOT%' && make dist"
    if errorlevel 1 (
        echo [ERREUR] make dist a echoue.
        echo   Dans le shell MSYS2 MINGW64:  cd %MROOT% ^&^& make dist
    ) else (
        echo.
        echo ============================================================
        echo  OK ! dist\ contient tdp.exe + portaudio.dll + README.md
        echo  Lancer :  run.bat   (ou dist\tdp.exe)
        echo ============================================================
    )
    goto :end
)

REM ---- Sinon : guider l'installation de MSYS2 ----
echo ============================================================
echo  MSYS2 / gcc introuvable.
echo
echo  ETAPES (2 minutes) :
echo   1. Telecharge MSYS2 :  https://www.msys2.org/
echo      (fichier msys2-x86_64-*.exe)
echo   2. Installe par defaut dans C:\msys64
echo   3. Menu demarrer -> "MSYS2 MINGW64"
echo   4. Dans ce shell :
echo        pacman -S --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-portaudio
echo   5. Puis :
echo        cd %MROOT%
echo        make dist
echo   6. Lancer : run.bat
echo
echo  OU relance ce script apres installation (il compilera direct).
echo ============================================================
REM Ouvre le shell MSYS2 MINGW64 si il existe.
if exist "%MSYS%\msys64.exe" (
    echo Overture du shell MSYS2...
    start "" "%MSYS%\msys64.exe" -c "msys2-launcher"
) else (
    echo (MSYS2 pas installe : telecharge-le sur https://www.msys2.org/ )
)

:end
endlocal
pause
