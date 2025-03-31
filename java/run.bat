@echo off
javac -cp jna.jar Test.java
java -cp "jna.jar;." Test "C:\Users\xungeng\AppData\Roaming\Tencent\xwechat\XPlugin\plugins\WeChatOcr\8020\extracted\wxocr.dll" "C:\Program Files\Tencent\Weixin\4.0.2.13" test.png
pause
