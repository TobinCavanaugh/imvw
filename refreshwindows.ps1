$code = '[DllImport("shell32.dll")] public static extern void SHChangeNotify(int
  eventId, int flags, IntPtr item1, IntPtr item2);'
Add-Type -MemberDefinition $code -Namespace Win32 -Name Shell
[Win32.Shell]::SHChangeNotify(0x08000000, 0x0000, [IntPtr]::Zero, [IntPtr]::Zero)