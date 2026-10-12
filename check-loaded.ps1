# media-player / check-loaded.ps1
#
# Proves that an install actually took effect: a library on disk says nothing
# about whether Explorer loaded it. It looks for a window of the mod's own class
# and for the taskbars, and prints where they are.
#
#   powershell -ExecutionPolicy Bypass -File check-loaded.ps1

$code = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public class WinScan {
    public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc lpEnumFunc, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);

    public class Found {
        public IntPtr Handle; public string Class; public string Rect;
        public bool Visible; public uint Pid;
    }

    public static List<Found> Find(string[] classes) {
        var result = new List<Found>();
        EnumWindows((h, l) => {
            var sb = new StringBuilder(256);
            GetClassName(h, sb, sb.Capacity);
            string cls = sb.ToString();
            foreach (var want in classes) {
                if (cls == want || cls.StartsWith(want)) {
                    RECT r; GetWindowRect(h, out r);
                    uint pid; GetWindowThreadProcessId(h, out pid);
                    result.Add(new Found {
                        Handle = h, Class = cls,
                        Rect = string.Format("{0},{1} - {2},{3}", r.Left, r.Top, r.Right, r.Bottom),
                        Visible = IsWindowVisible(h), Pid = pid
                    });
                    break;
                }
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
'@

Add-Type -TypeDefinition $code -ErrorAction Stop

$widgets = [WinScan]::Find(@('WindhawkTaskbarMediaWidget'))
if ($widgets.Count -eq 0) {
    Write-Host 'widget window : NOT FOUND'
    Write-Host '                 Explorer has not created it (mod not loaded, or still starting).'
} else {
    foreach ($w in $widgets) {
        Write-Host ("widget window : {0}  class={1}  rect={2}  visible={3}  pid={4}" -f `
            $w.Handle, $w.Class, $w.Rect, $w.Visible, $w.Pid)
    }
}

foreach ($cls in 'Shell_TrayWnd', 'Shell_SecondaryTrayWnd') {
    $found = [WinScan]::Find(@($cls))
    if ($found.Count -eq 0) { Write-Host ("{0,-24}: none" -f $cls) }
    foreach ($w in $found) {
        Write-Host ("{0,-24}: {1}  rect={2}  pid={3}" -f $cls, $w.Handle, $w.Rect, $w.Pid)
    }
}
