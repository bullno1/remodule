# Wait for a just-linked module's separate debug file to be completely
# written before the debugger reads it.
#
#   GDB 14+:  source wait-debug-info.py
#   LLDB 17+: command script import wait-debug-info.py
import ctypes
import os
import select
import struct
import time
import zlib

TIMEOUT = 5.0  # Seconds to wait for the debug file.
FRESH = 10.0   # Only a module modified within this many seconds is waited for.

IN_CLOSE_WRITE = 0x00000008
IN_MOVED_TO = 0x00000080
IN_NONBLOCK = 0o0004000
IN_CLOEXEC = 0o2000000

libc = ctypes.CDLL(None, use_errno=True)
report = print


def debug_link(path):
    """
    Name and CRC from .gnu_debuglink of a little-endian ELF64 file. None
    when it has no link, or has debug info of its own.
    """
    with open(path, "rb") as f:
        header = f.read(64)
        if header[:6] != b"\x7fELF\x02\x01":
            return None
        shoff, = struct.unpack_from("<Q", header, 0x28)
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", header, 0x3A)
        f.seek(shoff)
        table = f.read(shentsize * shnum)
        sections = [struct.unpack_from("<I20xQQ", table, i * shentsize) for i in range(shnum)]
        f.seek(sections[shstrndx][1])
        names = f.read(sections[shstrndx][2])
        link = None
        for name, offset, size in sections:
            section = names[name:names.index(b"\0", name)]
            if section == b".debug_info":
                return None
            if section == b".gnu_debuglink":
                f.seek(offset)
                data = f.read(size)
                link = data[:data.index(b"\0")].decode(), struct.unpack("<I", data[-4:])[0]
    return link


def file_crc(path):
    """CRC-32 of the file, or None when it cannot be read."""
    crc = 0
    try:
        with open(path, "rb") as f:
            while True:
                chunk = f.read(1 << 20)
                if not chunk:
                    return crc
                crc = zlib.crc32(chunk, crc)
    except OSError:
        return None


def wait_for_close(fd, name, deadline):
    """Whether a writer closed `name`, or something was renamed to it, before the deadline."""
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0 or not select.select([fd], [], [], remaining)[0]:
            return False
        try:
            events = os.read(fd, 65536)
        except BlockingIOError:
            continue
        offset = 0
        seen = False
        while offset < len(events):
            _, _, _, length = struct.unpack_from("iIII", events, offset)
            offset += 16
            seen = seen or events[offset:offset + length].rstrip(b"\0") == name
            offset += length
        if seen:
            return True


def wait_until_complete(path, crc):
    """Whether `path` has the wanted CRC, now or within TIMEOUT."""
    directory, name = os.path.split(path)
    fd = libc.inotify_init1(IN_NONBLOCK | IN_CLOEXEC)
    if fd < 0:
        return file_crc(path) == crc
    try:
        # The watch comes first, so a close during the check below is
        # queued instead of missed. The directory is watched because the
        # file may not exist yet.
        if libc.inotify_add_watch(fd, os.fsencode(directory), IN_CLOSE_WRITE | IN_MOVED_TO) < 0:
            return file_crc(path) == crc
        start = time.monotonic()
        checks = 0
        while True:
            checks += 1
            if file_crc(path) == crc:
                report("wait-for-debug-link: %s ready after %.0f ms, %d CRC %s" % (
                    name, (time.monotonic() - start) * 1000, checks, "pass" if checks == 1 else "passes"))
                return True
            if not wait_for_close(fd, os.fsencode(name), start + TIMEOUT):
                report("wait-for-debug-link: gave up on %s after %.0f ms" % (name, (time.monotonic() - start) * 1000))
                return False
    finally:
        os.close(fd)


checked = {}  # (module path, mtime) -> debug file path, or None


def debug_file_of(module):
    """Path of the module's complete debug file, or None to leave the lookup to the debugger."""
    try:
        mtime = os.stat(module).st_mtime
        if time.time() - mtime > FRESH:
            return None
        key = (os.path.abspath(module), mtime)
        if key in checked:
            return checked[key]
        link = debug_link(module)
    except OSError:
        return None
    checked[key] = None
    if link is not None:
        path = os.path.join(os.path.dirname(key[0]), link[0])
        if wait_until_complete(path, link[1]):
            checked[key] = path
    return checked[key]


try:
    import gdb
    import gdb.missing_debug
except ImportError:
    gdb = None

if gdb is not None:
    class WaitForDebugLink(gdb.missing_debug.MissingDebugHandler):
        def __init__(self):
            super().__init__("wait-for-debug-link")

        def __call__(self, objfile):
            return debug_file_of(objfile.filename)

    gdb.missing_debug.register_handler(None, WaitForDebugLink(), replace=True)


def locate_module(module_spec, module_file_spec, symbol_file_spec):
    import lldb
    module = module_spec.GetFileSpec().fullpath
    path = debug_file_of(module) if module is not None else None
    if path is not None:
        symbol_file_spec.SetDirectory(os.path.dirname(path))
        symbol_file_spec.SetFilename(os.path.basename(path))
    return lldb.SBError()


def __lldb_init_module(debugger, internal_dict):
    global report
    output = debugger.GetOutputFile()

    def report(msg):
        # print() from the callback does not reach the terminal.
        output.Write((msg + "\n").encode())
        output.Flush()

    debugger.GetSelectedPlatform().SetLocateModuleCallback(locate_module)
