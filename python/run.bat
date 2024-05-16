@echo off
if not exist "wcocr.pyd" (
  copy ..\vs.proj\x64\Debug\wcocr.dll wcocr.pyd
)
python test.py
pause
