#!/usr/bin/env python3
"""Generate a CycloneDX 1.6 JSON SBOM for a Kamailio build.

Invoked by the `sbom-cyclonedx` CMake target (see cmake/sbom-cyclonedx.cmake) with:
  --metadata   JSON file configured by CMake (product identity, module list)
  --artifacts  text file listing one built artifact path per line
  --binary-dir CMake binary dir (libraries under it are internal, skipped)
  --output     path of the CycloneDX JSON document to write

The linked-library inventory is read from the built artifacts with ldd
(otool -L on macOS) and each library is resolved to the distribution
package that owns it (dpkg-query, then rpm). Libraries that cannot be
resolved degrade to pkg:generic components with a warning on stderr;
resolution failures never make the target fail.

Only the Python standard library is used.
"""

import argparse
import datetime
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.parse
import uuid


def warn(msg):
    print("sbom-generate: warning: %s" % msg, file=sys.stderr)


def run(cmd):
    """Run a command, return its stdout or None on any failure."""
    try:
        proc = subprocess.run(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=True
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    return proc.stdout.decode("utf-8", errors="replace")


def os_release_id():
    try:
        with open("/etc/os-release") as f:
            for line in f:
                if line.startswith("ID="):
                    return line.split("=", 1)[1].strip().strip('"')
    except OSError:
        pass
    return "linux"


def linked_libs_ldd(artifact, binary_dir):
    """Return (soname, resolved path) pairs of one artifact, from ldd."""
    out = run(["ldd", artifact])
    if out is None:
        warn("ldd failed on %s" % artifact)
        return []
    pairs = []
    for line in out.splitlines():
        line = line.strip()
        if not line or line.startswith("linux-vdso"):
            continue
        if "=> not found" in line:
            warn("%s: %s" % (os.path.basename(artifact), line))
            continue
        if "=>" in line:
            soname = line.split("=>", 1)[0].strip()
            path = line.split("=>", 1)[1].strip().split(" (")[0].strip()
        else:
            # e.g. "/lib64/ld-linux-x86-64.so.2 (0x...)"
            path = line.split(" (")[0].strip()
            soname = os.path.basename(path)
        if not path.startswith("/"):
            continue
        real = os.path.realpath(path)
        if real.startswith(os.path.realpath(binary_dir) + os.sep):
            continue
        pairs.append((soname, real))
    return pairs


def linked_libs_otool(artifact, binary_dir):
    """Return (name, path) pairs of the direct dependencies, from otool -L."""
    out = run(["otool", "-L", artifact])
    if out is None:
        warn("otool failed on %s" % artifact)
        return []
    pairs = []
    for line in out.splitlines()[1:]:
        line = line.strip()
        if not line:
            continue
        path = line.split(" (")[0].strip()
        if path.startswith(("@rpath", "@loader_path", "@executable_path")):
            warn("%s: unresolved %s" % (os.path.basename(artifact), path))
            continue
        if path.startswith(("/usr/lib/", "/System/")):
            continue
        real = os.path.realpath(path)
        if real.startswith(os.path.realpath(binary_dir) + os.sep):
            continue
        pairs.append((os.path.basename(path), real))
    return pairs


def collect_libraries(artifacts, binary_dir):
    """Return (sorted library paths, soname -> path map) for all artifacts."""
    if platform.system() == "Darwin" or shutil.which("ldd") is None:
        discover = linked_libs_otool
    else:
        discover = linked_libs_ldd
    libs = set()
    soname_map = {}
    for artifact in artifacts:
        if not os.path.isfile(artifact):
            warn("artifact not found, skipped: %s" % artifact)
            continue
        for soname, path in discover(artifact, binary_dir):
            libs.add(path)
            soname_map[soname] = path
    return sorted(libs), soname_map


NEEDED_RE = re.compile(r"Shared library: \[([^\]]+)\]")


def dt_needed(path):
    """Return the DT_NEEDED sonames of one ELF object, from readelf -d."""
    out = run(["readelf", "-d", path])
    if out is None:
        return []
    return NEEDED_RE.findall(out)


def resolve_dpkg(path):
    out = run(["dpkg-query", "-S", path])
    if out is None:
        return None
    # "libssl3:amd64: /usr/lib/x86_64-linux-gnu/libssl.so.3"
    first = out.splitlines()[0]
    pkg = first.split(": ", 1)[0].split(",")[0].strip().split(":")[0]
    out = run(["dpkg-query", "-W", "-f", "${Package}\t${Version}\t${Architecture}", pkg])
    if out is None:
        return None
    name, version, arch = out.splitlines()[0].split("\t")
    purl = "pkg:deb/%s/%s@%s?arch=%s" % (
        os_release_id(),
        name,
        urllib.parse.quote(version, safe=""),
        arch,
    )
    return {"name": name, "version": version, "purl": purl}


def resolve_rpm(path):
    out = run(["rpm", "-qf", path, "--qf", "%{NAME}\t%{VERSION}-%{RELEASE}\t%{ARCH}\n"])
    if out is None:
        return None
    name, version, arch = out.splitlines()[0].split("\t")
    purl = "pkg:rpm/%s/%s@%s?arch=%s" % (
        os_release_id(),
        name,
        urllib.parse.quote(version, safe=""),
        arch,
    )
    return {"name": name, "version": version, "purl": purl}


def resolve_generic(path):
    base = os.path.basename(path)
    name = re.sub(r"\.(so|dylib).*$", "", base)
    if name.startswith("lib"):
        name = name[3:]
    component = {"name": name, "purl": "pkg:generic/%s" % name}
    match = re.search(r"\.so\.([0-9.]+)$", base)
    if match:
        component["version"] = match.group(1)
        component["purl"] += "@%s" % match.group(1)
    return component


def resolve_packages(libs):
    resolvers = []
    if shutil.which("dpkg-query"):
        resolvers.append(resolve_dpkg)
    if shutil.which("rpm"):
        resolvers.append(resolve_rpm)
    components = {}
    for path in libs:
        component = None
        for resolver in resolvers:
            component = resolver(path)
            if component:
                break
        if component is None:
            warn("no owning package found for %s, using pkg:generic" % path)
            component = resolve_generic(path)
        key = (component["name"], component.get("version", ""))
        entry = components.setdefault(key, (component, []))
        entry[1].append(path)
    result = []
    ref_by_path = {}
    for component, paths in (components[key] for key in sorted(components)):
        component["type"] = "library"
        component["scope"] = "required"
        component["bom-ref"] = component["purl"]
        component["properties"] = [
            {"name": "kamailio:resolved-libs", "value": ",".join(sorted(paths))}
        ]
        for path in paths:
            ref_by_path[path] = component["bom-ref"]
        result.append(component)
    return result, ref_by_path


def build_dependencies(main_ref, artifacts, libs, soname_map, ref_by_path, components):
    """Package-level dependency edges, from the DT_NEEDED entries of every
    inspected ELF object (artifacts and discovered libraries). Falls back to
    a flat graph when readelf is not available (e.g. macOS)."""
    if platform.system() == "Darwin" or shutil.which("readelf") is None:
        warn("readelf not available, emitting a flat dependency graph")
        return [{"ref": main_ref, "dependsOn": [c["bom-ref"] for c in components]}]

    def needed_refs(path):
        refs = set()
        for soname in dt_needed(path):
            target = soname_map.get(soname)
            if target in ref_by_path:
                refs.add(ref_by_path[target])
        return refs

    edges = {main_ref: set()}
    for component in components:
        edges.setdefault(component["bom-ref"], set())
    for artifact in artifacts:
        if os.path.isfile(artifact):
            edges[main_ref].update(needed_refs(artifact))
    for lib in libs:
        ref = ref_by_path.get(lib)
        if ref is not None:
            edges[ref].update(needed_refs(lib) - {ref})
    order = [main_ref] + [c["bom-ref"] for c in components]
    return [{"ref": ref, "dependsOn": sorted(edges[ref])} for ref in order]


def build_bom(meta, components, dependencies):
    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    if epoch:
        timestamp = datetime.datetime.fromtimestamp(
            int(epoch), tz=datetime.timezone.utc
        )
        serial = uuid.uuid5(
            uuid.NAMESPACE_URL,
            "kamailio@%s+%s" % (meta["version"], meta["git_hash"]),
        )
    else:
        timestamp = datetime.datetime.now(tz=datetime.timezone.utc)
        serial = uuid.uuid4()
    main_ref = meta["name"]
    return {
        "bomFormat": "CycloneDX",
        "specVersion": "1.6",
        "serialNumber": "urn:uuid:%s" % serial,
        "version": 1,
        "metadata": {
            "timestamp": timestamp.strftime("%Y-%m-%dT%H:%M:%SZ"),
            "tools": {
                "components": [
                    {
                        "type": "application",
                        "name": "kamailio-sbom-generate",
                        "version": meta["version"],
                    }
                ]
            },
            "component": {
                "type": "application",
                "bom-ref": main_ref,
                "name": meta["name"],
                "version": meta["version"],
                "purl": "pkg:generic/%s@%s"
                % (meta["name"], urllib.parse.quote(meta["version"], safe="")),
                "licenses": [{"license": {"id": "GPL-2.0-or-later"}}],
                "externalReferences": [
                    {"type": "vcs", "url": "https://github.com/kamailio/kamailio"},
                    {"type": "website", "url": "https://www.kamailio.org"},
                ],
                "properties": [
                    {"name": "kamailio:git-hash", "value": meta["git_hash"]},
                    {"name": "kamailio:git-state", "value": meta["git_state"]},
                    {"name": "kamailio:modules", "value": ",".join(meta["modules"])},
                    {"name": "kamailio:system", "value": "%s/%s"
                     % (meta["system_name"], meta["system_processor"])},
                    {"name": "kamailio:compiler", "value": "%s %s"
                     % (meta["c_compiler_id"], meta["c_compiler_version"])},
                    {"name": "kamailio:cmake", "value": meta["cmake_version"]},
                ],
            },
        },
        "components": components,
        "dependencies": dependencies,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--metadata", required=True)
    parser.add_argument("--artifacts", required=True)
    parser.add_argument("--binary-dir", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    with open(args.metadata) as f:
        meta = json.load(f)
    with open(args.artifacts) as f:
        artifacts = [line.strip() for line in f if line.strip()]

    libs, soname_map = collect_libraries(artifacts, args.binary_dir)
    components, ref_by_path = resolve_packages(libs)
    dependencies = build_dependencies(
        meta["name"], artifacts, libs, soname_map, ref_by_path, components
    )
    bom = build_bom(meta, components, dependencies)

    outdir = os.path.dirname(os.path.abspath(args.output))
    os.makedirs(outdir, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=outdir, suffix=".tmp")
    try:
        with os.fdopen(fd, "w") as f:
            json.dump(bom, f, indent=2)
            f.write("\n")
        os.replace(tmp, args.output)
    except BaseException:
        os.unlink(tmp)
        raise
    print(
        "sbom-generate: %d artifacts, %d libraries, %d components -> %s"
        % (len(artifacts), len(libs), len(components), args.output)
    )


if __name__ == "__main__":
    main()
