"""Read-only exit observation for a caller-specified disposable test process."""
import argparse
import ctypes
import json
from pathlib import Path
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--result", type=Path, required=True)
    args = parser.parse_args()
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
    kernel.OpenProcess.restype = ctypes.c_void_p
    kernel.QueryFullProcessImageNameW.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_ulong)]
    kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
    kernel.GetExitCodeProcess.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    process = kernel.OpenProcess(0x100000 | 0x1000, False, args.pid)
    if not process:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        name = ctypes.create_unicode_buffer(32768)
        length = ctypes.c_ulong(len(name))
        if not kernel.QueryFullProcessImageNameW(process, 0, name, ctypes.byref(length)):
            raise ctypes.WinError(ctypes.get_last_error())
        if Path(name.value).resolve() != args.executable.resolve():
            raise RuntimeError("The PID does not belong to the requested test executable")
        print(f"READY observing PID {args.pid}", flush=True)
        started = time.monotonic()
        waited = kernel.WaitForSingleObject(process, 45000)
        code = ctypes.c_ulong()
        if not kernel.GetExitCodeProcess(process, ctypes.byref(code)):
            raise ctypes.WinError(ctypes.get_last_error())
        result = {"pid": args.pid, "executable": name.value, "signaled": waited == 0, "exit_code": code.value, "observation_seconds": time.monotonic() - started, "observed_exit_unix": time.time()}
        args.result.parent.mkdir(parents=True, exist_ok=True)
        args.result.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result), flush=True)
        # Interpretation belongs to the caller: GUI hosts can use nonzero modal
        # return codes for ordinary exit. Preserve the observed code verbatim.
        if waited != 0:
            raise SystemExit(1)
    finally:
        kernel.CloseHandle(process)


if __name__ == "__main__":
    main()
