import com.sun.jna.Callback;
import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.WString;
import java.util.Collections;
import java.util.concurrent.atomic.AtomicReference;


public class Test {
    public interface WechatOCR extends Library {
        WechatOCR dll = (WechatOCR) Native.load("wcocr", WechatOCR.class,
                Collections.singletonMap(Library.OPTION_STRING_ENCODING, "UTF-8"));
        interface SetResCallback extends Callback {
            void callback(String arg);
        }
        boolean wechat_ocr(Object ocr_exe, Object wechat_dir, String imgfn, SetResCallback res);
        void stop_ocr();
        public static boolean call_wechat_ocr(String ocr_exe, String wechat_dir, String imgfn, SetResCallback res) {
			String os = System.getProperty("os.name").toLowerCase();
			boolean isWindows = os.contains("win");
			if (isWindows) {
				return dll.wechat_ocr(new WString(ocr_exe), new WString(wechat_dir), imgfn, res);
			} else {
				return dll.wechat_ocr(ocr_exe, wechat_dir, imgfn, res);
			}
		}
    }
    public static void main(String [] args) {
		if (args.length != 3) {
			System.out.println("usage: java ... Test ocr_exe_path wechat_dir_path png_path");
			return;
		}
        System.out.println("ocr begin...");
        String ocr_exe = args[0];
        String wechat_dir = args[1];
        String tstpng = args[2];
        AtomicReference<String> result = new AtomicReference<>();
        WechatOCR.call_wechat_ocr(ocr_exe, wechat_dir, tstpng, new WechatOCR.SetResCallback() {
            public void callback(String args) {
                result.set(args);
            }
        });
        System.out.println("OCR result: " + result.get());
        System.out.println("ocr end...");
        WechatOCR.dll.stop_ocr();
    }
}
