@echo off
REM ============================================================
REM  TDP - Installation Windows en 1 clic (double-clic)
REM  Telecharge MSYS2, l'installe en silence, installe gcc +
REM  PortAudio via pacman, puis compile tdp.exe dans dist\.
REM
REM  Pre-requis : connexion internet.
REM  A la fin : dist\tdp.exe + dist\portaudio.dll + dist\README.md
REM ============================================================
setlocal EnableExtensions
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "MSYS=C:\msys64"
set "PKG_URL=https://www.msys2.org/files/msys2-x86_64-latest.exe"
REM Chemin du repo en format MSYS (/c/...) pour que make/bash le trouvent.
set "MROOT=%ROOT:C:\=/c/%"
set "MROOT=%MROOT:\=/%"

echo ============================================================
echo  TDP - Installation Windows (1 clic)
echo ============================================================
cd /d "%ROOT%" || (echo Erreur: impossible de changer de dossier. & exit /b 1)
echo [info] repo : %ROOT%

REM ---- 1. Installer MSYS2 (silencieux) s'il n'est pas la ----
if not exist "%MSYS%\usr\bin\bash.exe" (
    echo [1/4] MSYS2 introuvable dans %MSYS% -> telechargement + installation...
    echo   (ca peut prendre 1-2 minutes)
    powershell -NoProfile -Command "Invoke-WebRequest -Uri '%PKG_URL%' -OutFile '%TEMP%\msys2-setup.exe'"
    if errorlevel 1 (
        echo [ERREUR] echec du telechargement de MSYS2.
        echo   Verifie ta connexion internet, ou installe MSYS2 a la main:
        echo   https://www.msys2.org/  puis relance ce script.
        exit /b 1
    )
    start /wait "" "%TEMP%\msys2-setup.exe" /S
    if not exist "%MSYS%\usr\bin\bash.exe" (
        echo [ERREUR] installation de MSYS2 echouee.
        echo   Installe MSYS2 a la main depuis https://www.msys2.org/
        echo   (par defaut C:\msys64), puis relance ce script.
        exit /b 1
    )
    echo [1/4] MSYS2 installe.
) else (
    echo [1/4] MSYS2 deja present dans %MSYS%.
)

REM ---- 2. Installer gcc + PortAudio via pacman (dans bash MSYS2) ----
echo [2/4] Installation de gcc (MinGW64) + PortAudio via pacman...
"%MSYS%\usr\bin\bash.exe" -lc "pacman -Sy --noconfirm && pacman -S --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-portaudio"
if errorlevel 1 (
    echo [AVERTISSEMENT] pacman a signale une erreur.
    echo   Le build peut quand meme marcher si gcc est deja la.
)

REM ---- 3. Verifier gcc ----
echo [3/4] Verification de gcc...
"%MSYS%\usr\bin\bash.exe" -lc "gcc --version | head -1"
if errorlevel 1 (
    echo [ERREUR] gcc introuvable apres installation.
    echo   Rouvre ce script, ou installe a la main via le shell 'MSYS2 MINGW64'.
    exit /b 1
)

REM ---- 4. Build -> dist\ ----
echo [4/4] Compilation de tdp.exe -> dist\ ...
"%MSYS%\usr\bin\bash.exe" -lc "cd '%MROOT%' && make dist"
if errorlevel 1 (
    echo [ERREUR] make dist a echoue.
    echo   Essaie a la main dans le shell 'MSYS2 MINGW64':
    echo     cd %MROOT%
    echo     make dist
    exit /b 1
)

echo.
echo ============================================================
echo  OK ! dist\ contient tdp.exe + portaudio.dll + README.md
echo  Lancer :  run.bat   (ou dist\tdp.exe)
echo ============================================================
endlocal
pause
