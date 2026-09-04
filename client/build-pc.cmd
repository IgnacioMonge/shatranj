@echo off
rem Interactive shim for the canonical PC client dev build+test.
rem Incremental: safe to re-run; only changed sources rebuild.
rem Packaging/deploy for release stays in build-msvc.ps1 (make client).
setlocal
pwsh -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-dev.ps1" || exit /b 1
