@echo off

:: call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
:: set clangLoc = "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/Llvm/bin/"
set clangLoc="C:/Program Files/LLVM/bin/"
cmake -DCMAKE_C_COMPILER=%clangLoc%clang.exe -DCMAKE_CXX_COMPILER=%clangLoc%clang++.exe -DCMAKE_C_FLAGS="-v -m64 -fcolor-diagnostics -fansi-escape-codes" -DCMAKE_CXX_FLAGS="-m64 -fcolor-diagnostics -fansi-escape-codes" -DCMAKE_TOOLCHAIN_FILE=C:\tools\vcpkg\scripts\buildsystems\vcpkg.cmake -G"Ninja" ..
