using System;
using System.Runtime.InteropServices;
using System.Text;

class Program
{
    // 注意要把生成的64位dll拷到 c_sharp\bin\x64\Debug\net8.0 目录下
    // 声明外部函数
    [DllImport("wcocr.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern bool wechat_ocr(
        [MarshalAs(UnmanagedType.LPWStr)] string ocr_exe,
        [MarshalAs(UnmanagedType.LPWStr)] string wechat_dir,
        [MarshalAs(UnmanagedType.LPStr)] string imgfn,
        SetResultDelegate set_res);

    // 定义委托类型
    public delegate void SetResultDelegate(IntPtr result);

    public class StringResult
    {
        public string result_ = "";
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

    static void Main(string[] args)
    {
        if (args.Length != 3)
        {
            Console.WriteLine("Usage: wcocr.exe ocr_exe wechat_dir imgfn");
            return;
        }

        StringResult stringResult = new StringResult();
        SetResultDelegate setRes = new SetResultDelegate(stringResult.SetResult);
        bool success = wechat_ocr(args[0], args[1], args[2], setRes);
        Console.WriteLine($"OCR Success: {success} res:{stringResult.GetResult()}");
    }
}
