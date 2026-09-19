import socket
import json, struct


def send_display_command(payload: dict, timeout: float = 0.5) -> None:
    body = json.dumps(payload).encode("utf-8")
    header = struct.pack("!I", len(body))
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.settimeout(timeout)
            sock.connect("/run/jr-hyaku/jr-hyaku.sock")
            sock.sendall(header + body)
    except (ConnectionError, socket.timeout, FileNotFoundError):
        pass
