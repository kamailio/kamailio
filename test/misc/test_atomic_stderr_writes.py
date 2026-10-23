#!/usr/bin/env python3
"""Stress the actual ksr_stderr_printf() helper extracted from src/core/dprint.c.

Requires a C compiler and Python 3; deliberately avoids Kamailio link deps.
This is a helper-level regression test, not the full Kamailio integration suite.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "src/core/dprint.c").read_text()
start = source.index("void ksr_stderr_printf(const char *format, ...)")
braces = source.index("{", start)
depth = 0
end = None
for i in range(braces, len(source)):
    if source[i] == "{":
        depth += 1
    elif source[i] == "}":
        depth -= 1
        if depth == 0:
            end = i + 1
            break
assert end is not None
helper = source[start:end]

headers = """#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
"""
driver = r"""
int main(void) {
    const int children = 4, per_child = 1000;
    int pipefds[2];
    if (pipe(pipefds) != 0) return 2;
    for (int k = 0; k < children; ++k) {
        pid_t pid = fork();
        if (pid < 0) return 3;
        if (pid == 0) {
            close(pipefds[0]);
            if (dup2(pipefds[1], STDERR_FILENO) < 0) _exit(4);
            close(pipefds[1]);
            char payload[321];
            memset(payload, 'A'+k, 320);
            payload[320] = 0;
            for (int n=0; n < per_child; ++n)
                ksr_stderr_printf("worker=%d sequence=%04d %s\n", k, n, payload);
            _exit(0);
        }
    }
    close(pipefds[1]);
    char buf[16384];
    size_t used=0, count=0;
    while (1) {
        ssize_t n = read(pipefds[0], buf + used, sizeof(buf) - used);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return 5;
        if (!n) break;
        used += n;
        size_t start = 0;
        for (size_t i=0; i < used; i++) {
            if (buf[i] != '\n') continue;
            size_t len = i-start+1;
            if (len < 342 || len > 360) return 6;
            char line[512], payload[322];
            int worker=-1, sequence=-1;
            memcpy(line, buf+start, len);
            line[len]=0;
            if (sscanf(line, "worker=%d sequence=%d %321s",
                       &worker, &sequence, payload) != 3) return 7;
            if (worker<0 || worker>=children || sequence<0 || sequence>=per_child ||
                strlen(payload)!=320) return 8;
            for (int j=0; j<320; j++) if (payload[j]!='A'+worker) return 9;
            ++count;
            start=i+1;
        }
        if (start) {
            memmove(buf,buf+start,used-start);
            used -= start;
        }
        if (used==sizeof(buf)) return 10;
    }
    close(pipefds[0]);
    for (int k=0;k<children;k++) {
        int status;
        if (wait(&status)<0 || !WIFEXITED(status) || WEXITSTATUS(status)) return 11;
    }
    if (used || count != (size_t)(children*per_child)) return 12;
    puts("PASS: 4000 multi-process records were not interleaved");
    return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / "stress.c"
    binary = Path(tmp) / "stress"
    src.write_text(headers + "\n" + helper + "\n" + driver)
    subprocess.run(
        ["cc", "-std=c11", "-D_DEFAULT_SOURCE", "-Wall", "-Wextra", "-O2",
         "-o", str(binary), str(src)], check=True
    )
    subprocess.run([str(binary)], check=True)
