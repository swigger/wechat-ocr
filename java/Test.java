import com.sun.jna.Callback;
import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.WString;
import java.util.Collections;
import java.util.concurrent.atomic.AtomicReference;


public class Test {
    public interface WechatOCR extends Library {
        WechatOCR dll = (WechatOCR) Native.load("../vs.proj/x64/Debug/wcocr", WechatOCR.class,
                Collections.singletonMap(Library.OPTION_STRING_ENCODING, "UTF-8"));
        interface SetResCallback extends Callback {
            void callback(String arg);
        }
        boolean wechat_ocr(WString ocr_exe, WString wechat_dir, String imgfn, SetResCallback res);
    }
    public static void main(String [] args) {
        System.out.println("ocr begin...");
        String ocr_exe = "C:\\Users\\xungeng\\AppData\\Roaming\\Tencent\\WeChat\\XPlugin\\Plugins\\WeChatOCR\\7079\\extracted\\WeChatOCR.exe";
        String wechat_dir = "C:\\Program Files (x86)\\Tencent\\WeChat\\[3.9.10.19]";
        String tstpng = "test.png";
        AtomicReference<String> result = new AtomicReference<>();
        WechatOCR.dll.wechat_ocr(new WString(ocr_exe), new WString(wechat_dir), tstpng, new WechatOCR.SetResCallback() {
            public void callback(String args) {
                result.set(args);
            }
        });
        System.out.println("OCR result: " + result.get());
        System.out.println("ocr end...");
    }
}
