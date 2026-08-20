@echo off
REM ---------------------------------------------------------------------------
REM Build and run the PixelScale filter tests.
REM
REM Deliberately outside dr2.sln: this compiles interface\pixelscale.cpp on its
REM own against a stubbed IFace, so it cannot break - or be broken by - the game
REM build.
REM
REM pixelscale.cpp is staged into build\ rather than compiled in place because
REM MSVC resolves a quoted #include relative to the including file's directory
REM first. Compiled in place it would find the real interface\iface.h and drag
REM the whole engine in; from build\ it finds the stub.
REM ---------------------------------------------------------------------------

setlocal
set HERE=%~dp0
set ROOT=%HERE%..\..

if not exist "%HERE%build" mkdir "%HERE%build"
copy /y "%ROOT%\interface\pixelscale.cpp" "%HERE%build\pixelscale.cpp" >nul
if errorlevel 1 (
    echo Could not stage interface\pixelscale.cpp
    exit /b 1
)

REM Find a compiler if we are not already in a developer prompt
where cl >nul 2>&1
if not errorlevel 1 goto :havecl

set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VSWHERE%" goto :nocompiler

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -prerelease -property installationPath`) do set VSPATH=%%i
if not defined VSPATH goto :nocompiler

call "%VSPATH%\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1

where cl >nul 2>&1
if not errorlevel 1 goto :havecl

:nocompiler
echo No Visual Studio C++ compiler found. Run this from a developer prompt.
exit /b 1

:havecl

pushd "%HERE%"
cl /nologo /EHsc /W3 ^
   /I"%HERE%stub" /I"%HERE%build" /I"%ROOT%\interface" ^
   pstest.cpp /Fo:build\ /Fe:build\pstest.exe
if errorlevel 1 (
    popd
    echo Compilation failed.
    exit /b 1
)
popd

"%HERE%build\pstest.exe"
exit /b %errorlevel%
