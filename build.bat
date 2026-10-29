@echo off
rem build.bat - locate VS Build Tools' vcvarsall.bat via vswhere, load the
rem cl.exe environment, then run make with whatever args were passed in.
rem
rem Usage: build.bat            (builds default target)
rem        build.bat clean      (runs "make clean")

setlocal

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%VSWHERE%" (
    echo Could not find vswhere.exe at "%VSWHERE%".
    echo Is Visual Studio Installer / Build Tools installed?
    exit /b 1
)

set "VSINSTALL="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VSINSTALL=%%i"
)

if not defined VSINSTALL (
    echo Could not locate a VS install with the C++ build tools component.
    echo Check that "Desktop development with C++" / VC.Tools.x86.x64 is installed.
    exit /b 1
)

call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 (
    echo vcvarsall.bat failed.
    exit /b 1
)

make %*
