import wcocr
import os

if os.name == 'nt':
    wechat_path = r"C:\Program Files\Tencent\Weixin\4.0.2.13"
    wechatocr_path = os.getenv("APPDATA") + r"\Tencent\WeChat\XPlugin\Plugins\WeChatOCR\7079\extracted\WeChatOCR.exe"
else:
    wechat_path = "/opt/wechat"
    wechatocr_path = "/opt/wechat/wxocr"

wcocr.init(wechatocr_path, wechat_path)
result = wcocr.ocr("../demo/test呀哈哟.png")
print(result)
