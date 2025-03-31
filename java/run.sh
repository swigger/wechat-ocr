#!/bin/sh
if [ ! -f libwcocr.so ] ; then
	if [ ! -f ../build/libwcocr.so ] ; then
		echo "please build project first! (using cmake)" >&2
		exit 1
	fi
	ln -sf ../build/libwcocr.so .
fi
javac -cp jna.jar Test.java
java -cp "jna.jar:." Test "/opt/wechat/wxocr"  "/opt/wechat" test.png
