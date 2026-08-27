#!/usr/bin/env python3
"""
check_header_layout.py  —  Python 3.8+, no external dependencies.

Verifies that every target which compiles a converted target's public header
actually declares a link to it, and that a converted dependency named from a
PUBLIC header is exposed PUBLIC rather than PRIVATE.

Why this exists
---------------
The public/private header migration (CONVENTIONS.md, "Target Layout") moves each
target's API to include/<Target>/**, exposed only through a <Target>_headers
INTERFACE handle. A converted header therefore STOPS resolving through the
global ${CMAKE_SOURCE_DIR}/Source include root: <Logger/Logger.h> compiles only
where Logger_headers is reachable.

That is the point of the migration, but it means every conversion can silently
break a consumer that had been getting the header for free. Two checks catch it:

  MISSING_LINK   a target compiles <Converted/...> but nothing in its link
                 closure provides that include dir.

  PRIVATE_LEAK   a target names <Converted/...> from one of its own PUBLIC
                 headers while declaring the dependency PRIVATE. It compiles
                 fine itself; its consumers do not inherit the dir and break at
                 a distance. Three real instances were found this way
                 (RendererFrontend -> ShaderSystem, ModuleVideo -> VideoSystem,
                 ECS -> Logger), only one of which the build had surfaced.
                 The fix is to PUBLIC-link the _headers handle and leave the
                 real target PRIVATE: headers propagate, linkage does not.

Both checks are needed together. Walking only each target's own files misses
requirements that arrive transitively through an UNCONVERTED target's headers
(this is exactly how a "clean" audit was followed by a failed build); ignoring
CMake's PUBLIC-link propagation reports false positives instead.

This is a static reading of the CMakeLists files, not a configure. It is
deliberately dependency-free and fast so it can run before asking for a build.

Usage:
    python Tools/check_header_layout.py           # report and exit non-zero on findings
    python Tools/check_header_layout.py --quiet   # exit code only
    python Tools/check_header_layout.py --list    # also list the converted targets

Add a target here the moment it is converted; CONVERTED is the single source of
truth for "which prefixes no longer resolve through the global Source/ root".
"""

import argparse
import os
import re
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
SOURCE_DIR = ROOT_DIR / "Source"

# Converted targets: include prefix -> public include dir, relative to Source/.
# Keep in step with CONVENTIONS.md and the engine DLL's PUBLIC link list in
# Source/Engine/CMakeLists.txt.
CONVERTED = {
    "Logger":              "Engine/Core/Logger/include",
    "MemoryManager":       "Engine/Core/MemoryManager/include",
    "FileSystem":          "Engine/Core/FileSystem/include",
    "NOUS_Multithreading": "Engine/NOUS_Multithreading/include",
    "AudioSystem":         "Engine/Systems/AudioSystem/include",
    "VideoSystem":         "Engine/Systems/VideoSystem/include",
    "ShaderSystem":        "Engine/Systems/ShaderSystem/include",
    "CameraSystem":        "Engine/Systems/CameraSystem/include",
    "ECS":                 "Engine/Systems/ECS/include",
    "ResourceManager":     "Engine/Systems/ResourceManager/include",
    "PrefabManager":       "Engine/Systems/PrefabManager/include",
}

# Targets that re-export every converted _headers handle PUBLIC. Anything linking
# these inherits the whole set, which is why the ~60 test executables need no
# declarations of their own. See the PUBLIC block in Source/Engine/CMakeLists.txt.
ENGINE_UMBRELLAS = {"NousEngine", "Nous-Engine", "NousEngine::Engine",
                    "Nous-Editor", "NousEngine::Editor"}

SOURCE_SUFFIXES = (".h", ".cpp", ".inl")
HEADER_SUFFIXES = (".h", ".inl")

INCLUDE_RE = re.compile(r'#include\s*[<"]([^">]+)[">]')
COMMENT_RE = re.compile(r"#[^\n]*")
ADD_LIB_RE = re.compile(
    r"add_library\(\s*(\$\{CLASS_NAME\}|\$\{PROJECT_NAME\}|[A-Za-z0-9_:.-]+)"
    r"\s+(?:STATIC|SHARED|INTERFACE|MODULE)")
ADD_EXE_RE = re.compile(
    r"add_executable\(\s*(\$\{TEST_NAME\}|\$\{PROJECT_NAME\}|[A-Za-z0-9_:.-]+)")
LINK_RE = re.compile(
    r"target_link_libraries\(\s*([^\s)]+)\s+(PUBLIC|PRIVATE|INTERFACE)"
    r"((?:[^()]|\([^()]*\))*)\)")
NAME_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_:.-]*")


def posix(path):
    return str(path).replace(os.sep, "/")


class Tree:
    """Static model of the source tree: include graph + CMake target graph."""

    def __init__(self, source_dir):
        self.source_dir = Path(source_dir)
        self.text = {}          # posix path -> file contents
        self.targets = {}       # posix dir  -> [target names declared there]
        self.cmake = {}         # posix dir  -> comment-stripped CMakeLists text
        self.links = {}         # target     -> {"PUBLIC": set, "PRIVATE": set}
        self._needs = {}        # memoised include-closure per file
        self._load_sources()
        self._load_cmake()

    # ---------------------------------------------------------------- loading
    def _load_sources(self):
        for dirpath, _, filenames in os.walk(self.source_dir):
            for name in filenames:
                if name.endswith(SOURCE_SUFFIXES):
                    path = Path(dirpath) / name
                    rel = posix(path.relative_to(self.source_dir))
                    self.text[rel] = path.read_text(encoding="utf-8", errors="replace")

    def _load_cmake(self):
        for dirpath, _, filenames in os.walk(self.source_dir):
            if "CMakeLists.txt" not in filenames:
                continue
            path = Path(dirpath) / "CMakeLists.txt"
            rel_dir = posix(path.parent.relative_to(self.source_dir))
            # Strip comments FIRST: a ')' inside one truncates LINK_RE's block.
            body = COMMENT_RE.sub("", path.read_text(encoding="utf-8", errors="replace"))
            self.cmake[rel_dir] = body

            class_name = self._setting(body, "CLASS_NAME")
            test_name = self._setting(body, "TEST_NAME")

            def expand(raw):
                return (raw.replace("${CLASS_NAME}", class_name or raw)
                           .replace("${TEST_NAME}", test_name or raw)
                           .replace("${PROJECT_NAME}", "NousEngine"))

            # A directory may declare several targets (Editor/UI has two).
            names = [expand(m) for m in ADD_LIB_RE.findall(body)]
            names += [expand(m) for m in ADD_EXE_RE.findall(body)]
            names = [n for n in names if "${" not in n]
            if names:
                self.targets[rel_dir] = names

            for match in LINK_RE.finditer(body):
                target = expand(match.group(1))
                scope = "PUBLIC" if match.group(2) in ("PUBLIC", "INTERFACE") else "PRIVATE"
                entry = self.links.setdefault(target, {"PUBLIC": set(), "PRIVATE": set()})
                entry[scope].update(NAME_RE.findall(match.group(3)))

    @staticmethod
    def _setting(body, key):
        match = re.search(r"set\(" + key + r'\s+"([^"]+)"', body)
        return match.group(1) if match else None

    # ------------------------------------------------------------- resolution
    def _resolve(self, spec, current):
        """Map an #include spec to a file in the tree, or None if external."""
        prefix = spec.split("/")[0]
        if prefix in CONVERTED:
            candidate = posix(Path(CONVERTED[prefix]) / spec)
            return candidate if candidate in self.text else None
        for candidate in (posix(Path(spec)),
                          posix(Path(current).parent / spec)):
            candidate = posix(os.path.normpath(candidate))
            if candidate in self.text:
                return candidate
        return None

    def needs(self, path, stack=()):
        """Converted prefixes reachable from `path` through the include graph."""
        if path in self._needs:
            return self._needs[path]
        if path in stack:                      # include cycle guard
            return set()
        found = set()
        for spec in INCLUDE_RE.findall(self.text[path]):
            prefix = spec.split("/")[0]
            if prefix in CONVERTED:
                found.add(prefix)
            resolved = self._resolve(spec, path)
            if resolved and resolved.endswith(HEADER_SUFFIXES):
                found |= self.needs(resolved, stack + (path,))
        self._needs[path] = found
        return found

    def chain_to(self, path, prefix, stack=()):
        """Shortest-ish include chain from `path` to a header of `prefix`.

        Reported with every finding: these requirements are usually transitive
        (an editor window needing ShaderSystem through ResourceShader.h), and
        without the chain the reader has to re-derive it by hand -- which also
        hides the fact that the right fix is often on the INTERMEDIATE target,
        not the one being reported.
        """
        if path in stack or prefix not in self.needs(path):
            return None
        for spec in INCLUDE_RE.findall(self.text[path]):
            if spec.split("/")[0] == prefix:
                return [path, spec]
            resolved = self._resolve(spec, path)
            if resolved and resolved.endswith(HEADER_SUFFIXES):
                tail = self.chain_to(resolved, prefix, stack + (path,))
                if tail:
                    return [path] + tail
        return None

    # ------------------------------------------------------------ link graph
    def provides(self, lib, seen=None):
        """Converted prefixes a CONSUMER inherits by linking `lib`.

        Only PUBLIC/INTERFACE edges propagate, which is what CMake does and what
        keeps this from reporting false positives on PRIVATE-linked internals.
        """
        seen = set() if seen is None else seen
        if lib in seen:
            return set()
        seen.add(lib)
        if lib in ENGINE_UMBRELLAS:
            return set(CONVERTED)
        found = set()
        for prefix in CONVERTED:
            if lib in (prefix, prefix + "_headers", prefix + "_private"):
                found.add(prefix)
        for dep in self.links.get(lib, {}).get("PUBLIC", ()):
            found |= self.provides(dep, seen)
        return found

    def visible_to(self, target):
        """Prefixes `target` can compile against: its own dirs plus every dep."""
        found = {p for p in CONVERTED if target == p}
        edges = self.links.get(target, {})
        for dep in edges.get("PUBLIC", set()) | edges.get("PRIVATE", set()):
            found |= self.provides(dep)
        return found

    def public_scope_of(self, target):
        """Prefixes `target` re-exports to its consumers."""
        found = {p for p in CONVERTED if target == p}
        for dep in self.links.get(target, {}).get("PUBLIC", ()):
            found |= self.provides(dep)
        return found

    def owner_of(self, path):
        """Nearest enclosing directory that declares a target."""
        current = posix(Path(path).parent)
        while True:
            if current in self.targets:
                return current
            parent = posix(Path(current).parent)
            if parent == current or current in (".", ""):
                return None
            current = parent


def audit(tree):
    """Return (missing_link, private_leak) findings."""
    required = {}                    # target dir -> set of prefixes needed
    for path in tree.text:
        prefixes = tree.needs(path)
        if prefixes:
            owner = tree.owner_of(path)
            if owner is not None:
                required.setdefault(owner, set()).update(prefixes)

    missing = []
    for owner, prefixes in sorted(required.items()):
        names = tree.targets[owner]
        visible = set()
        for name in names:
            visible |= tree.visible_to(name)
        gap = sorted(prefixes - visible)
        if gap:
            missing.append((names[0], owner, gap))

    leaks = []
    for owner, names in sorted(tree.targets.items()):
        exported = set()
        for name in names:
            exported |= tree.public_scope_of(name)
        for path in tree.text:
            if not path.endswith(HEADER_SUFFIXES):
                continue
            if tree.owner_of(path) != owner:
                continue
            # Only a header that ships as API can leak: tests and src/ cannot.
            parts = path.split("/")
            if "test" in parts or "src" in parts:
                continue
            for prefix in sorted(tree.needs(path) - exported):
                leaks.append((names[0], path, prefix, tree.chain_to(path, prefix)))
    return missing, leaks


def main():
    parser = argparse.ArgumentParser(
        description="Check converted-header link declarations across the tree.")
    parser.add_argument("--quiet", action="store_true",
                        help="print nothing; communicate through the exit code")
    parser.add_argument("--list", action="store_true",
                        help="list the converted targets before auditing")
    args = parser.parse_args()

    if not SOURCE_DIR.is_dir():
        print(f"error: {SOURCE_DIR} not found", file=sys.stderr)
        return 2

    tree = Tree(SOURCE_DIR)
    missing, leaks = audit(tree)

    if args.quiet:
        return 1 if (missing or leaks) else 0

    if args.list:
        print(f"Converted targets ({len(CONVERTED)}):")
        for prefix, include_dir in sorted(CONVERTED.items()):
            print(f"  <{prefix}/...>  ->  Source/{include_dir}")
        print()

    if missing:
        print("MISSING_LINK - compiles a converted header without a link that provides it:")
        for target, owner, prefixes in missing:
            print(f"  {target}  ({owner})")
            for prefix in prefixes:
                print(f"      needs {prefix}_headers")
        print()

    if leaks:
        print("PRIVATE_LEAK - names a converted target from a PUBLIC header "
              "without exposing it PUBLIC:")
        for target, path, prefix, chain in leaks:
            print(f"  {target}: Source/{path}")
            print(f"      add {prefix}_headers to a PUBLIC target_link_libraries block")
            if chain and len(chain) > 2:
                print("      via " + " -> ".join(chain[1:]))
        print()

    total = len(missing) + len(leaks)
    print(f"{total} finding(s); {len(tree.text)} files, {len(tree.targets)} target dirs scanned.")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
