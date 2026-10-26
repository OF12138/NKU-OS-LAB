param([string]$Out, [int]$Cols = 190, [int]$Lines = 50, [int]$Wait = 3, [string]$Title = "ucore")
# 打开一个 Windows Terminal 窗口（标签标题为 -Title）attach 到 WSL 中的 tmux 会话 t2，用 PrintWindow 截取该窗口后关闭
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  public struct RECT { public int L, T, R, B; }
  public struct POINT { public int X, Y; }
}
"@
[W]::SetProcessDPIAware() | Out-Null
Start-Process wt.exe -ArgumentList "-w new --size $Cols,$Lines new-tab --title $Title --suppressApplicationTitle wsl.exe -e tmux attach -t t2"
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 50 -and $h -eq [IntPtr]::Zero; $i++) {
  Start-Sleep -Milliseconds 200
  Add-Type -AssemblyName UIAutomationClient
  $cond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::NameProperty, $Title)
  $e = [System.Windows.Automation.AutomationElement]::RootElement.FindFirst([System.Windows.Automation.TreeScope]::Children, $cond)
  if ($e) { $h = [IntPtr]$e.Current.NativeWindowHandle }
}
if ($h -eq [IntPtr]::Zero) { throw "找不到 conhost 窗口" }
Start-Sleep -Seconds $Wait
$r = New-Object W+RECT; [W]::GetWindowRect($h, [ref]$r) | Out-Null
$c = New-Object W+RECT; [W]::GetClientRect($h, [ref]$c) | Out-Null
$o = New-Object W+POINT; [W]::ClientToScreen($h, [ref]$o) | Out-Null
$bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc(); [W]::PrintWindow($h, $hdc, 2) | Out-Null; $g.ReleaseHdc($hdc); $g.Dispose()
# 只保留客户区（去掉标题栏和边框）
$rect = New-Object System.Drawing.Rectangle ($o.X - $r.L), ($o.Y - $r.T), $c.R, $c.B
$bmp.Clone($rect, $bmp.PixelFormat).Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
[W]::PostMessage($h, 0x10, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
"saved $Out  client=$($c.R)x$($c.B)"
