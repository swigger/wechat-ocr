# wcocr: demostrate how to use wechatocr.exe

Greate thanks to IEEE by his [Project IEEE/QQImpl](https://github.com/EEEEhex/qqimpl)] and [article](https://mp.weixin.qq.com/s?__biz=MjM5NTc2MDYxMw==&mid=2458516946&idx=1&sn=3de89283cb61c7a0cab08bc79cb4d83d).
This project is based on it. 

This project reduced the product size by using protobuf-lite instead of protobuf, 
and provided a direct Python interface for calling in sync mode.

To use this project, you need to prepare the [wechatocr.exe] and the wechat folder.
For example, wechatocr.exe might be:
```
C:\Users\yourname\AppData\Roaming\Tencent\WeChat\XPlugin\Plugins\WeChatOCR\7061\extracted\WeChatOCR.exe
```
and the wechat folder might be:
```
C:\Program Files (x86)\Tencent\WeChat\[3.9.8.25]
```

## c++ interface
You can use the following code to test it:
```cpp
CWeChatOCR ocr(wechatocr_path, wechat_path);
if (!ocr.wait_connection(5000)) {
	// error handling
}
CWeChatOCR::result_t result;
ocr.doOCR("D:\\test.png", &result);
```
You can also pass nullptr to the second parameter of `doOCR` to call in async mode and wait the callback.
In this case, you need to subclass `CWeChatOCR` and implement the virtual function `OnOCRResult`.

## python interface
Rename the built wcocr.dll to wcocr.pyd and put it in the same directory as wcocr.py.
You can use the following code to test it:
```python
import wcocr
wcocr.init(wechatocr_path, wechat_path)
result = wcocr.ocr("D:\\test.png")
```

Currently, the python interface only supports sync mode.
