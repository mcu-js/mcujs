"""Exercise the FatFs harness's dependency-fetch stage, without network/compiler."""
import contextlib
import hashlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import urllib.error
import zipfile

ROOT = Path(__file__).resolve().parents[1]
# Execute the actual fetch/extract entry stage; never substitute production code.
SOURCE = (ROOT / 'scripts/test-storage-fatfs.sh').read_text().split("<<'PY'\n", 1)[1].split('\nPY\n', 1)[0]
HTTPS = 'https://elm-chan.org/fsw/ff/arc/ff16.zip'
HTTP = 'http://elm-chan.org/fsw/ff/arc/ff16.zip'


class FatFsDownloadTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.out = self.root / 'out'
        self.out.mkdir()
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, 'w') as archive:
            for name in ['ff.c', 'ff.h', 'diskio.h', 'ffunicode.c']:
                archive.writestr('source/' + name, 'fixture-' + name)
        self.archive = stream.getvalue()
        self.digest = hashlib.sha256(self.archive).hexdigest()
        (self.root / 'Dockerfile').write_text('ENV FATFS_SHA256=' + self.digest + '\n')

    def run_stage(self, responses, offline=''):
        with patch('sys.argv', ['fetch', str(self.root), str(self.out), offline]), \
             patch('urllib.request.urlopen', side_effect=responses) as request, \
             contextlib.redirect_stdout(io.StringIO()):
            exec(compile(SOURCE, 'actual-fatfs-fetch-stage', 'exec'), {'__name__': '__main__'})
        return request

    def test_https_timeout_falls_back_to_same_author_archive(self):
        request = self.run_stage([urllib.error.URLError(TimeoutError()), io.BytesIO(self.archive)])
        self.assertEqual([call.args[0] for call in request.call_args_list], [HTTPS, HTTP])
        for name in ['ff.c', 'ff.h', 'diskio.h', 'ffunicode.c']:
            self.assertEqual((self.out / name).read_text(), 'fixture-' + name)

    def test_corrupt_fallback_is_rejected_before_extraction(self):
        with self.assertRaisesRegex(SystemExit, 'checksum mismatch'):
            self.run_stage([urllib.error.URLError(TimeoutError()), io.BytesIO(b'corrupt')])
        self.assertEqual(list(self.out.iterdir()), [])

    def test_corrupt_https_is_not_retried_or_extracted(self):
        with self.assertRaisesRegex(SystemExit, 'checksum mismatch'):
            self.run_stage([io.BytesIO(b'corrupt')])
        self.assertEqual(list(self.out.iterdir()), [])

    def test_pinned_offline_archive_needs_no_network(self):
        offline = self.root / 'offline.zip'
        offline.write_bytes(self.archive)
        request = self.run_stage([], str(offline))
        request.assert_not_called()
        self.assertEqual((self.out / 'ff.c').read_text(), 'fixture-ff.c')


if __name__ == '__main__':
    unittest.main()
