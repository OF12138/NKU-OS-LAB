@echo off
rem Double-click: kill QEMU and both gdb; the Windows Terminal panes close by themselves.
wsl.exe --cd "%~dp0." -e bash ./dual-gdb.sh stop
