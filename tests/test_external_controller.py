# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
"""Exercise a separate controller process, loss, restart and no local takeover."""
from pathlib import Path
import ctypes
import json
import queue
import subprocess
import sys
import threading
import time


def main():
    binary = Path(sys.argv[1]).resolve()
    library = ctypes.CDLL(str(binary.parent / "slx-solarpilot-core.dll"))
    library.slx_sp_create.argtypes = [ctypes.c_uint32]
    library.slx_sp_create.restype = ctypes.c_void_p
    library.slx_sp_destroy.argtypes = [ctypes.c_void_p]
    library.slx_sp_dispatch.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t,
                                      ctypes.c_void_p, ctypes.c_size_t]
    context = library.slx_sp_create(1)
    if not context:
        raise RuntimeError("Cannot create plant core")
    buffer = ctypes.create_string_buffer(2 * 1024 * 1024)
    process = None

    def call(request):
        payload = json.dumps(request).encode()
        code = library.slx_sp_dispatch(context, payload, len(payload), buffer, len(buffer))
        value = json.loads(buffer.value)
        if code:
            raise RuntimeError(value)
        return value

    def start(endpoint):
        child = subprocess.Popen([str(binary), "--ems-listen", endpoint],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 text=True, creationflags=subprocess.CREATE_NO_WINDOW)
        ready = queue.Queue()
        threading.Thread(target=lambda: ready.put(child.stdout.readline()), daemon=True).start()
        try:
            message = ready.get(timeout=10)
            result = json.loads(message)
            assert result["role"] == "ems-reference"
            return child, result["endpoint"]
        except Exception:
            child.terminate()
            child.wait(timeout=5)
            raise

    def stop(child):
        child.terminate()
        child.wait(timeout=5)

    try:
        process, endpoint = start("tcp://127.0.0.1:0")
        call({"op": "connect_controller", "endpoint": endpoint})
        call({"op": "configure", "patch": {"ev_mode": "fast"}})
        state = call({"op": "advance", "steps": 3})
        assert state["link"]["fresh"] and state["link"]["external"]
        assert state["link"]["controller_source"] == "ems-reference"
        assert state["ev_w"] > 0
        session = state["link"]["session"]
        packets = call({"op": "packets"})
        assert {p["direction"] for p in packets} == {"PC TX", "PC RX"}
        command = call({"op": "last_command"})
        stop(process)
        process = None
        state = call({"op": "advance", "steps": 2})
        assert not state["link"]["fresh"]
        assert state["ev_w"] == 0 and state["battery_w"] == 0
        assert state["link"]["external"], "The plant silently fell back to its local EMS"
        process, _ = start(endpoint)
        for _ in range(20):
            state = call({"op": "advance"})
            if state["link"]["fresh"]:
                break
            time.sleep(.05)
        assert state["link"]["fresh"] and state["link"]["session"] == session
        assert state["link"]["controller_source"] == "ems-reference"
        assert call({"op": "verify_command", "command": command})["status"].startswith("rejected:")
        call({"op": "connect_controller", "endpoint": "local"})
        state = call({"op": "advance"})
        assert not state["link"]["external"] and state["link"]["fresh"]
        assert state["link"]["controller_source"] == "pc-ems-local"
        print("Separate EMS process / CBOR TCP / loss / restart / explicit local return: passed")
    finally:
        if process is not None:
            stop(process)
        library.slx_sp_destroy(context)


if __name__ == "__main__":
    main()
