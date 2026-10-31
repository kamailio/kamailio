#!/usr/bin/env python3
"""Helper-level regression for Kamailio stderr logging (issue #4980).

Compiles the real ksr_stderr_printf() implementation extracted from dprint.c.
Checks EINTR, short writes, malloc-backed formatting, and JSON/normal output.
This is NOT a substitute for an end-to-end Kamailio -E/-DD process test.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "src/core/dprint.c").read_text()
signature = "void ksr_stderr_printf(const char *format, ...)"
start = source.index(signature)
brace_start = source.index("{", start)
depth = 0
end = None
for idx in range(brace_start, len(source)):
    if source[idx] == "{":
        depth += 1
    elif source[idx] == "}":
        depth -= 1
        if depth == 0:
            end = idx + 1
            break
assert end is not None, "C helper closing brace not found"
helper = source[start:end].replace(
    "write(STDERR_FILENO, buf + off, (size_t)size - off)",
    "test_write(STDERR_FILENO, buf + off, (size_t)size - off)",
)
assert "test_write(STDERR_FILENO" in helper, "write() call changed"

headers = r"""
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* Faults are injected only for the first stderr record. */
static int writes = 0;
static ssize_t test_write(int fd, const void *buf, size_t n) {
    ++writes;
    if (writes == 1) { errno = EINTR; return -1; }
    if (writes == 2) return write(fd, buf, n > 7 ? 7 : n);
    return write(fd, buf, n);
}
"""
driver = r"""
int main(void) {
    char *big = malloc(9001);
    if (!big) return 10;
    memset(big, 'Z', 9000);
    big[9000] = 0;
    ksr_stderr_printf("small %d %s\n", 42, "record");
    ksr_stderr_printf("%s\n", big);
    ksr_stderr_printf("{\"event\":\"%s\",\"status\":%d}\n", "call", 200);
    free(big);
    if (writes < 5) return 11; /* EINTR and a partial write were handled. */
    return 0;
}
"""
with tempfile.TemporaryDirectory() as directory:
    src = Path(directory) / "stderr_edge.c"
    exe = Path(directory) / "stderr_edge"
    src.write_text(headers + "\n" + helper + "\n" + driver)
    subprocess.run(
        ["cc", "-std=c11", "-D_DEFAULT_SOURCE", "-Wall", "-Wextra",
         "-Werror", "-O2", "-o", str(exe), str(src)],
        check=True,
    )
    proc = subprocess.run([str(exe)], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, check=True)
    want = ("small 42 record\n" + "Z" * 9000 + "\n" +
            '{"event":"call","status":200}\n').encode()
    assert proc.stderr == want, (
        f"record mismatch: {len(proc.stderr)} bytes vs {len(want)}"
    )
    assert proc.stdout == b"", "unexpected stdout"
print("PASS: EINTR, short write, heap allocation, classic + JSON output")
