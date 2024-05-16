@echo off
javac -cp jna.jar Test.java
java -cp "jna.jar;." Test
pause
