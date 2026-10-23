# SPDX-License-Identifier: Apache-2.0
"""Regression checks for hashing large website downloads without loading them into RAM."""

import hashlib
from pathlib import Path
import tempfile
import tracemalloc
import unittest

from remote_deploy import digest


class DigestTests(unittest.TestCase):
    def test_large_download_has_bounded_memory(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'download.bin'
            block = bytes(range(256)) * 4096
            expected = hashlib.sha256()
            with path.open('wb') as output:
                for _ in range(256):
                    output.write(block)
                    expected.update(block)

            tracemalloc.start()
            try:
                actual = digest(path)
                _, peak = tracemalloc.get_traced_memory()
            finally:
                tracemalloc.stop()

            self.assertEqual(actual, expected.hexdigest())
            self.assertLess(peak, 8 * 1024 * 1024, 'A 256 MiB download must not be read into RAM')

    def test_empty_file(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'empty.bin'
            path.touch()
            self.assertEqual(digest(path), hashlib.sha256(b'').hexdigest())


if __name__ == '__main__':
    unittest.main()
