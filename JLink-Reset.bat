@echo off
call paths.bat
JLink.exe -Device CORTEX-M3 -If SWD -Speed 1000 USDK\flasher\RTL_Reset.JLinkScript
