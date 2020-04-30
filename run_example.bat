@echo off

echo [examples] Which example? [triangle ^| deferred (default)]
set /P _name= || set "_name=deferred"
cd "../examples/%_name%"
echo [examples] running %_name%.exe (wd: "../examples/%_name%")
C:\dev\projects\ovk\build\bin\%_name%.exe
echo [examples] finished %_name%.exe
cd ../../build
