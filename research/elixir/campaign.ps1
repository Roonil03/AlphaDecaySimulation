param(
    [Parameter(Mandatory=$true)][string]$CampaignDir,
    [Parameter(Mandatory=$true)][string]$Manifest,
    [Parameter(Mandatory=$true)][string]$Scheduler,
    [Parameter(Mandatory=$true)][string]$Core,
    [int]$Workers = 1
)
$ErrorActionPreference = 'Stop'
# A Windows job object kills the entire process tree when this wrapper exits,
# including an abrupt termination. Start suspended, attach, then resume: an
# unattached orphan can never write to the campaign.
Add-Type -TypeDefinition @"
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
public static class CampaignJob {
    [StructLayout(LayoutKind.Sequential)] struct Basic {
        public long PerProcessUserTimeLimit, PerJobUserTimeLimit;
        public uint LimitFlags;
        public UIntPtr MinimumWorkingSetSize, MaximumWorkingSetSize;
        public uint ActiveProcessLimit;
        public UIntPtr Affinity;
        public uint PriorityClass, SchedulingClass;
    }
    [StructLayout(LayoutKind.Sequential)] struct IO {
        public ulong ReadOperationCount, WriteOperationCount, OtherOperationCount;
        public ulong ReadTransferCount, WriteTransferCount, OtherTransferCount;
    }
    [StructLayout(LayoutKind.Sequential)] struct Extended {
        public Basic BasicLimitInformation;
        public IO IoInfo;
        public UIntPtr ProcessMemoryLimit, JobMemoryLimit, PeakProcessMemoryUsed, PeakJobMemoryUsed;
    }
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct Startup {
        public int cb; public string reserved, desktop, title;
        public int x, y, xSize, ySize, xChars, yChars, fill, flags;
        public short show, reserved2; public IntPtr reservedPtr, stdin, stdout, stderr;
    }
    [StructLayout(LayoutKind.Sequential)] struct ProcessInfo {
        public IntPtr process, thread; public uint processId, threadId;
    }
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern IntPtr CreateJobObject(IntPtr attributes, string name);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool SetInformationJobObject(IntPtr job, int kind, ref Extended info, uint length);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool CreateProcess(string application, StringBuilder command, IntPtr pa, IntPtr ta, bool inherit, uint flags, IntPtr env, string cwd, ref Startup start, out ProcessInfo process);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
    [DllImport("kernel32.dll", SetLastError=true)] static extern uint ResumeThread(IntPtr thread);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool TerminateProcess(IntPtr process, uint code);
    [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr process, out uint code);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    static void Check(bool ok) { if (!ok) throw new Win32Exception(Marshal.GetLastWin32Error()); }
    public static int Run(string command, string cwd) {
        IntPtr job = CreateJobObject(IntPtr.Zero, null);
        if (job == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
        ProcessInfo pi = new ProcessInfo();
        try {
            Extended limits = new Extended(); limits.BasicLimitInformation.LimitFlags = 0x2000;
            Check(SetInformationJobObject(job, 9, ref limits, (uint)Marshal.SizeOf(typeof(Extended))));
            Startup si = new Startup(); si.cb = Marshal.SizeOf(typeof(Startup));
            Check(CreateProcess(null, new StringBuilder(command), IntPtr.Zero, IntPtr.Zero, true, 4, IntPtr.Zero, cwd, ref si, out pi));
            if (!AssignProcessToJobObject(job, pi.process)) { TerminateProcess(pi.process, 1); throw new Win32Exception(Marshal.GetLastWin32Error()); }
            if (ResumeThread(pi.thread) == 0xffffffff) { TerminateProcess(pi.process, 1); throw new Win32Exception(Marshal.GetLastWin32Error()); }
            WaitForSingleObject(pi.process, 0xffffffff);
            uint result; Check(GetExitCodeProcess(pi.process, out result)); return (int)result;
        } finally {
            if (pi.thread != IntPtr.Zero) CloseHandle(pi.thread);
            if (pi.process != IntPtr.Zero) CloseHandle(pi.process);
            CloseHandle(job);
        }
    }
}
"@
New-Item -ItemType Directory -Force -Path $CampaignDir | Out-Null
$lockPath = Join-Path $CampaignDir 'owner.lock'
try {
    $lock = [System.IO.File]::Open($lockPath, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
} catch { throw "Campaign is already owned or lock cannot be opened: $lockPath" }
try {
    $mix = (Get-Command mix.bat -ErrorAction Stop).Source
    $parts = @($mix, 'run', (Join-Path $PSScriptRoot 'run.exs'), $CampaignDir, $Manifest, $Scheduler, $Core, "$Workers")
    foreach ($part in $parts) {
        if ($part -match '[%!"]') { throw 'Campaign paths cannot contain percent, exclamation or quote characters.' }
    }
    $quoted = ($parts | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $command = 'cmd.exe /d /s /c "' + $quoted + '"'
    $code = [CampaignJob]::Run($command, (Get-Location).Path)
    if ($code -ne 0) { exit $code }
} finally { $lock.Dispose() }
