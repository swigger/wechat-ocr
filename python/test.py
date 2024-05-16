import wcocr
import os

wechat_path = r"C:\Program Files (x86)\Tencent\WeChat\[3.9.10.19]"
wechatocr_path = os.getenv("APPDATA") + r"\Tencent\WeChat\XPlugin\Plugins\WeChatOCR\7079\extracted\WeChatOCR.exe"
wcocr.init(wechatocr_path, wechat_path)
result = wcocr.ocr("../java/test.png")
print(result)
