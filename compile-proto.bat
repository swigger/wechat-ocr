@echo off
cd /d %~dp0
cd src
..\spt\protoc.exe --cpp_out=lite:. -I ../pb ocr_common.proto ocr_wx3.proto ocr_wx4.proto
pause
