# SPDX-License-Identifier: Apache-2.0

"""Client identity and bounded MCP tool-call accounting."""

from collections import deque
from datetime import datetime, timezone
from functools import wraps
import inspect
import time


def client_name(ctx):
    if ctx is not None:
        try:
            params = ctx.session.client_params
            if params:
                return params.clientInfo.name[:128]
            request = ctx.request_context.request
            if request is not None:
                return request.headers.get("user-agent", "unidentified")[:128]
        except (AttributeError, ValueError):
            pass
    return "unidentified"


class CallAudit:
    def __init__(self, lock):
        self.lock = lock
        self.entries = deque(maxlen=1000)
        self.sequence = 0

    def begin(self, name, arguments):
        summary = {}
        for key, value in arguments.items():
            if key == "ctx":
                continue
            if key == "payload":
                summary["payload_length"] = len(value)
            elif key == "config":
                summary[key] = {k: v for k, v in value.items() if k in ("kind", "port", "host", "network_port", "bind_port")}
            else:
                summary[key] = str(value)[:128] if isinstance(value, str) else value
        with self.lock:
            self.sequence += 1
            entry = {
                "seq": self.sequence, "time": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
                "client": client_name(arguments.get("ctx")), "tool": name,
                "arguments": summary, "status": "running", "duration_ms": 0, "error": "",
            }
            self.entries.append(entry)
            return entry

    def finish(self, entry, started, error=""):
        with self.lock:
            entry["duration_ms"] = round((time.monotonic() - started) * 1000, 1)
            entry["status"] = "error" if error else "ok"
            entry["error"] = error[:300]

    def snapshot(self):
        with self.lock:
            return [dict(entry) for entry in self.entries]

    def wrap(self, function):
        def start(kwargs):
            return self.begin(function.__name__, kwargs), time.monotonic()

        if inspect.iscoroutinefunction(function):
            @wraps(function)
            async def wrapped(**kwargs):
                entry, started = start(kwargs)
                try:
                    result = await function(**kwargs)
                except Exception as exc:
                    self.finish(entry, started, str(exc))
                    raise
                self.finish(entry, started)
                return result
        else:
            @wraps(function)
            def wrapped(**kwargs):
                entry, started = start(kwargs)
                try:
                    result = function(**kwargs)
                except Exception as exc:
                    self.finish(entry, started, str(exc))
                    raise
                self.finish(entry, started)
                return result
        return wrapped
