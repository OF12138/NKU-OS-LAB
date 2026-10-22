@echo off
rem Double-click: open Windows Terminal with a QEMU tab and a split GDB tab
rem (left: gdb attached to QEMU, right: gdb for ucore). See dual-gdb.sh.
set "T=%~dp0."
wsl.exe --cd "%T%" -e bash ./dual-gdb.sh stop >nul 2>&1
wt.exe -w new -M new-tab --title QEMU --suppressApplicationTitle wsl.exe --cd "%T%" -e bash ./dual-gdb.sh qemu ; new-tab --title GDB --suppressApplicationTitle wsl.exe --cd "%T%" -e bash ./dual-gdb.sh qemugdb ; split-pane -V --title ucore wsl.exe --cd "%T%" -e bash ./dual-gdb.sh ucoregdb ; move-focus left
