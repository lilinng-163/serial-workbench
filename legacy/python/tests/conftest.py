import os
import time

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

import pytest

from serial_assistant.engine import Engine


def eventually(check, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = check()
        if result:
            return result
        time.sleep(0.02)
    raise AssertionError("Timed out waiting for condition")


@pytest.fixture
def engine():
    instance = Engine()
    yield instance
    instance.close()
