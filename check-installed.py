# media-player / check-installed.py
#
# Answers one question with evidence: is the build being tested actually the
# build in the source, or an older one that happens to be installed?
#
# It does not trust the file name or the version in the registry - Windhawk names
# the library after the version it was built from, and the source can be edited
# without reinstalling. It compares the internal symbols of the installed library
# against symbols that only exist after a given change, and reports the time of
# the last install against the time the source was last edited.
#
#   python check-installed.py

import glob
import os
import re
import sys
import datetime

MODS_DIR = r"C:\ProgramData\Windhawk\Engine\Mods\64"
SOURCE = r"D:\projects\media-player\taskbar-media-widget.wh.cpp"

# A symbol that exists only when the visualizer is compiled in. The capture
# thread is a good witness: it has to exist as its own function because
# CreateThread takes its address, so no amount of optimisation can fold it away.
# The others are listed because they survived -O1 in this build; several of the
# small helpers are inlined and are deliberately not listed.
WITNESS = "CaptureThreadProcEPv"
ALSO_VISUALIZER = [
    "ReadScopeFrame",
    "IsFloatFormatEPK11WAVEFORMATEX",
    "DrawMeterERN7Gdiplus",
    "ScopeAnalyser",
    "g_captureWorks",
]

# The 2.6.0 responsiveness work, still present.
ALSO_2_6_0 = [
    "StateForRendering",
    "CommandThreadProcEPv",
    "ReportPlaybackEventE",
    "SetOptimisticStateE",
    "g_commandThreadExitedEventE",
    "g_optimisticMutexE",
    "g_playbackEventSeenE",
]

SYMBOL = re.compile(rb"_ZN12_GLOBAL__N_1(\d+)([A-Za-z_][A-Za-z0-9_]*)")


def stamp(path):
    return datetime.datetime.fromtimestamp(os.path.getmtime(path)).strftime(
        "%Y-%m-%d %H:%M:%S")


def symbols(path):
    data = open(path, "rb").read()
    return {m.group(2).decode() for m in SYMBOL.finditer(data)}


def main():
    libs = sorted(glob.glob(os.path.join(MODS_DIR, "taskbar-media-widget*.dll")),
                  key=os.path.getmtime)
    if not libs:
        print("no mod library found in %s" % MODS_DIR)
        return 1

    print("source   : %s   (edited %s)"
          % (SOURCE, stamp(SOURCE) if os.path.exists(SOURCE) else "missing"))
    for lib in libs:
        print("installed: %s   (%d bytes, built %s)"
              % (lib, os.path.getsize(lib), stamp(lib)))
    print()

    newest = libs[-1]
    have = symbols(newest)
    presentVisualizer = [name for name in ALSO_VISUALIZER
                         if any(name in s for s in have)]
    present = [name for name in ALSO_2_6_0
               if any(s.startswith(name) for s in have)]
    witness = any(s.startswith(WITNESS) for s in have)

    print("symbols of the installed build that only the visualizer build has:")
    if not presentVisualizer and not witness:
        print("  none")
    else:
        if witness:
            print("  %s   (the loopback capture thread)" % WITNESS)
        for name in presentVisualizer:
            print("  %s" % name)
    print()
    if witness:
        if not present:
            print("VERDICT: the visualizer is installed, but the responsiveness")
            print("         symbols are missing, which was not expected; check")
            print("         the source.")
            return 1
        print("VERDICT: the installed build contains the visualizer.")
        return 0

    if os.path.exists(SOURCE) and os.path.getmtime(SOURCE) > os.path.getmtime(newest):
        print("VERDICT: the source is NEWER than the installed library, so the fix")
        print("         is in the source but nothing is running it. The behaviour")
        print("         being tested is the OLD build's behaviour.")
        print()
        print("         Install it, elevated:")
        print("           powershell -ExecutionPolicy Bypass "
              "-File D:\\projects\\media-player\\force-install.ps1")
        print("         Then run this check again and retest.")
        return 1

    print("VERDICT: the installed build does not contain the visualizer, and the")
    print("         source is not newer. Check that the source really carries the")
    print("         change.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
