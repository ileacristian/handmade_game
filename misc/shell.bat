@echo off
call subst w: C:\workspace >nul 2>&1
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
set path=w:\handmade\misc;%path%