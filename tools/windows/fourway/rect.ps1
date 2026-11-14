# [TEMP] fourway: window and client rectangles of every BF2.exe main window (interactive session).
Add-Type @"
using System; using System.Runtime.InteropServices;
public struct R { public int l, t, r, b; }
public static class W {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out R r);
  [DllImport("user32.dll")] public static extern int GetDpiForWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetWindowDpiAwarenessContext(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetAwarenessFromDpiAwarenessContext(IntPtr c);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
[W]::SetProcessDPIAware() | Out-Null
Get-Process BF2 -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | ForEach-Object {
  $h = $_.MainWindowHandle; $w = New-Object R; $c = New-Object R
  [W]::GetWindowRect($h, [ref]$w) | Out-Null; [W]::GetClientRect($h, [ref]$c) | Out-Null
  $aw = [W]::GetAwarenessFromDpiAwarenessContext([W]::GetWindowDpiAwarenessContext($h))
  "$($_.Path) window=$($w.r-$w.l)x$($w.b-$w.t) at $($w.l),$($w.t) client=$($c.r)x$($c.b) dpi=$([W]::GetDpiForWindow($h)) awareness=$aw"
} | Out-File -Encoding ascii C:\rendfw\rect.txt
