"""Compile generated cJSON examples as C and C++, then test values and allocation failures."""
# SPDX-License-Identifier: Apache-2.0
import argparse
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def literal(text):
    return '"' + ''.join(chr(byte) if 32 <= byte < 127 and byte not in (34, 92)
                         else '\\%03o' % byte for byte in text.encode('utf-8')) + '"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cjson-dir', type=Path, default=ROOT / 'build/cjson-test')
    parser.add_argument('--fixtures', type=Path, default=ROOT / 'build/cjson-fixtures')
    args = parser.parse_args()
    cache = (ROOT / 'build/native-release/CMakeCache.txt').read_text(encoding='utf-8')
    compiler = next(line.split('=', 1)[1] for line in cache.splitlines() if line.startswith('CMAKE_CXX_COMPILER:'))
    compiler_bin = Path(compiler).parent
    gcc = compiler_bin / 'gcc.exe'
    environment = dict(os.environ)
    environment['PATH'] = str(compiler_bin) + ';' + environment['PATH']
    library = args.fixtures / 'cjson.o'
    subprocess.run([str(gcc), '-std=c99', '-DCJSON_HIDE_SYMBOLS', '-c', str(args.cjson_dir / 'cJSON.c'),
                    '-o', str(library)], env=environment, check=True)
    results = []
    for fixture in sorted(path for path in args.fixtures.iterdir() if path.is_dir()):
        expected = (fixture / 'expected.json').read_text(encoding='utf-8')
        wrong_root = '[]' if isinstance(json.loads(expected), dict) else '{}'
        source = '''#include <stdlib.h>
#include <string.h>
#include "parse.c"
#include "build.c"
static int calls = 0;
static int limit = -1;
static int live = 0;
static void *tracked_malloc(size_t size)
{
    void *pointer;
    if (limit >= 0 && calls++ >= limit) { return NULL; }
    pointer = malloc(size);
    if (pointer != NULL) { ++live; }
    return pointer;
}
static void tracked_free(void *pointer)
{
    if (pointer != NULL) { --live; free(pointer); }
}
int main(void)
{
    cJSON *expected = cJSON_Parse(EXPECTED);
    char *text = build_message();
    cJSON *actual = text != NULL ? cJSON_Parse(text) : NULL;
    int equal = cJSON_Compare(expected, actual, 1);
    cJSON_Hooks hooks = {tracked_malloc, tracked_free};
    int parsed = text != NULL ? parse_message(text) : -1;
    cJSON_Delete(expected);
    cJSON_Delete(actual);
    cJSON_free(text);
    if (!equal || parsed != 0) { return 1; }
    if (parse_message("not-json") == 0 || parse_message(WRONG_ROOT) == 0) { return 2; }
    cJSON_InitHooks(&hooks);
    for (limit = 0; limit < 1024; ++limit)
    {
        calls = 0;
        text = build_message();
        if (text != NULL)
        {
            cJSON_free(text);
            if (live != 0) { return 3; }
            break;
        }
        if (live != 0) { return 4; }
    }
    if (limit >= 1024) { return 5; }
    for (limit = 0; limit < 1024; ++limit)
    {
        calls = 0;
        parsed = parse_message(EXPECTED);
        if (live != 0) { return 6; }
        if (parsed == 0) { break; }
    }
    if (limit >= 1024) { return 7; }
    cJSON_InitHooks(NULL);
    return 0;
}
'''.replace('EXPECTED', literal(expected)).replace('WRONG_ROOT', literal(wrong_root))
        harness = fixture / 'harness.c'
        harness.write_text(source, encoding='utf-8')
        for language, executable, standard in [('C', gcc, 'c99'), ('C++', Path(compiler), 'c++17')]:
            output = fixture / ('check-c.exe' if language == 'C' else 'check-cpp.exe')
            subprocess.run([str(executable), f'-std={standard}', '-Wall', '-Wextra', '-Werror',
                            '-DCJSON_HIDE_SYMBOLS', '-I', str(args.cjson_dir), str(harness), str(library),
                            '-o', str(output)], env=environment, check=True, capture_output=True, text=True)
            subprocess.run([str(output)], env=environment, check=True, timeout=10)
            results.append({'fixture': fixture.name, 'language': language, 'passed': True})
            print(f'{fixture.name}: {language} passed', flush=True)
    if not results:
        raise SystemExit('Generate fixtures with json_tools_tests and SW_CJSON_FIXTURE_DIR first.')
    report = {'passed': True, 'checks': results, 'allocation_failure_checks': True}
    (args.fixtures / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
