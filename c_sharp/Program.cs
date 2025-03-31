using System;
using System.Runtime.InteropServices;
using System.Text;
using static Program;

class WechatOCR
{
    // 注意要把生成的64位dll拷到 c_sharp\bin\x64\Debug\net8.0 目录下
    delegate void SetResultDelegate(IntPtr result);

    [DllImport("wcocr.dll", CallingConvention = CallingConvention.Cdecl)]
    static extern bool wechat_ocr(
        [MarshalAs(UnmanagedType.LPWStr)] string ocr_exe,
        [MarshalAs(UnmanagedType.LPWStr)] string wechat_dir,
        byte[] imgfn,
        SetResultDelegate set_res);

    [DllImport("wcocr.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void stop_ocr();

    class StringResult
    {
        string result_ = "";
        public void SetResult(IntPtr dt)
        {
            int length = 0;
            while (Marshal.ReadByte(dt, length) != 0) length++;
            byte[] byteArray = new byte[length];
            Marshal.Copy(dt, byteArray, 0, length);
            result_ = Encoding.UTF8.GetString(byteArray);
        }
        public string GetResult()
        {
            return result_;
        }
    };

    public static bool call_wechat_ocr(string ocr_exe, string wechat_dir, string imgfn, out string res)
    {
        StringResult stringResult = new StringResult();
        SetResultDelegate setRes = new SetResultDelegate(stringResult.SetResult);
        bool success = wechat_ocr(ocr_exe, wechat_dir, Encoding.UTF8.GetBytes(imgfn + "\0"), setRes);
        res = stringResult.GetResult();
        return success;
    }
};

class Program
{
    static void Main(string[] args)
    {
        if (args.Length != 3)
        {
            Console.WriteLine("Usage: wcocr.exe ocr_exe wechat_dir imgfn");
            return;
        }
        string sRes;
        bool success = WechatOCR.call_wechat_ocr(args[0], args[1], args[2], out sRes);
        Console.WriteLine($"OCR Success?:{success} res:{sRes}");
        WechatOCR.stop_ocr();
    }
}
