@echo off
rem Compila Dustfall su Windows (richiede CMake e Visual Studio 2022 con "Sviluppo di applicazioni desktop con C++")
cmake -S . -B build -A x64 || goto :error
cmake --build build --config Release --parallel || goto :error
if not exist release mkdir release
copy /Y build\Release\Dustfall.exe release\Dustfall.exe >nul
echo.
echo Fatto! Avvia release\Dustfall.exe
pause
exit /b 0

:error
echo.
echo Errore durante la compilazione. Servono CMake e Visual Studio 2022 (C++).
pause
exit /b 1
