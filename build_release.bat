@echo off
setlocal enabledelayedexpansion
cd /d "C:\Documents\NEw project\Kaigen Phantom\build"
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe" KaigenPhantom.sln /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
