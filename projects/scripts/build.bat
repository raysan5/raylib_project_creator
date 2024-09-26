::@echo off
:: > Setup required Environment
:: -------------------------------------
set COMPILER_DIR=C:\raylib\w64devkit\bin
set PATH=%PATH%;%COMPILER_DIR%
cd %~dp0
:: .
:: > Compile simple .rc file
:: ----------------------------
cmd /c windres raylib_project_creator.rc -o raylib_project_creator.rc.data
:: .
:: > Generating project
:: --------------------------
cmd /c mingw32-make -f Makefile ^
PROJECT_NAME=raylib_project_creator ^
PROJECT_VERSION=1.0 ^
PROJECT_DESCRIPTION="My cool project" ^
PROJECT_INTERNAL_NAME=raylib_project_creator ^
PROJECT_PLATFORM=PLATFORM_DESKTOP ^
PROJECT_SOURCE_FILES="raylib_project_creator.c" ^
BUILD_MODE="RELEASE" ^
BUILD_WEB_ASYNCIFY=FALSE ^
BUILD_WEB_MIN_SHELL=TRUE ^
BUILD_WEB_HEAP_SIZE=268435456 ^
RAYLIB_MODULE_AUDIO=FALSE ^
RAYLIB_MODULE_MODELS=FALSE ^
