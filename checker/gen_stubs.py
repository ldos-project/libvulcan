"""
Generate stub vulcan headers for vulcan_checker.py.

Parses the real vulcan headers with libclang to extract:
  - listener struct names + constructor signatures (global.hpp / object.hpp)
  - public method names + return types for feature_store and rank_config

Writes minimal stub headers into ./stubs/ with no stdlib dependencies.
Run this whenever a listener or public method is added.
"""

from __future__ import annotations
from pathlib import Path
import clang.cindex as cx
cx.Config.set_library_file('/usr/lib/llvm-18/lib/libclang.so')

ROOT           = Path(__file__).parent.parent
INCLUDE_DIR    = ROOT / "include"
STUBS_DIR      = ROOT / "stubs"
CLANG_RESOURCE = "/usr/lib/llvm-18/lib/clang/18/include"

PARSE_ARGS = [
    "-x", "c++", "-std=c++17",
    f"-I{INCLUDE_DIR}",
    f"-isystem{CLANG_RESOURCE}",
    "-ferror-limit=0",
]

_VOID_KINDS = {cx.TypeKind.VOID}
_BOOL_KINDS = {cx.TypeKind.BOOL}
_INT_KINDS  = {cx.TypeKind.INT, cx.TypeKind.LONG, cx.TypeKind.LONGLONG,
               cx.TypeKind.UINT, cx.TypeKind.ULONG, cx.TypeKind.ULONGLONG}


def _parse(header: Path) -> cx.TranslationUnit:
    return cx.Index.create().parse(
        str(header), args=PARSE_ARGS,
        options=cx.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD,
    )


# ---------------------------------------------------------------------------
# Listener extraction
# ---------------------------------------------------------------------------

def _constructor_params(struct_cursor: cx.Cursor) -> list[str] | None:
    for child in struct_cursor.get_children():
        if child.kind == cx.CursorKind.CONSTRUCTOR and not child.is_default_method():
            return [p.type.spelling for p in child.get_arguments()]
    return None


def extract_listeners(header: Path, cpp_namespace: str) -> list[tuple[str, list[str] | None]]:
    results: list[tuple[str, list[str] | None]] = []

    def walk(cursor: cx.Cursor) -> None:
        if cursor.kind == cx.CursorKind.STRUCT_DECL and cursor.spelling:
            parts, c = [], cursor
            while c and c.kind != cx.CursorKind.TRANSLATION_UNIT:
                if c.spelling: parts.append(c.spelling)
                c = c.semantic_parent
            if "::".join(reversed(parts)).startswith(cpp_namespace + "::"):
                results.append((cursor.spelling, _constructor_params(cursor)))
        for child in cursor.get_children():
            walk(child)

    walk(_parse(header).cursor)
    return results


# ---------------------------------------------------------------------------
# Class method extraction
# ---------------------------------------------------------------------------

def _return_hint(cursor: cx.Cursor) -> str:
    """Map a method's real return type to a safe primitive for the stub."""
    kind = cursor.result_type.get_canonical().kind
    if kind in _VOID_KINDS: return "void"
    if kind in _BOOL_KINDS: return "bool"
    if kind in _INT_KINDS:  return "long long"
    return "double"


def extract_public_methods(header: Path, class_name: str) -> list[tuple[str, str, bool]]:
    """Return [(method_name, return_hint, is_const)] for each unique public method."""
    seen:    set[str]                   = set()
    results: list[tuple[str, str, bool]] = []

    def walk(cursor: cx.Cursor) -> None:
        if cursor.kind in (cx.CursorKind.CLASS_DECL, cx.CursorKind.STRUCT_DECL):
            if cursor.spelling == class_name:
                for child in cursor.get_children():
                    if child.kind not in (cx.CursorKind.CXX_METHOD,
                                          cx.CursorKind.FUNCTION_TEMPLATE):
                        continue
                    if child.access_specifier != cx.AccessSpecifier.PUBLIC:
                        continue
                    name = child.spelling
                    if name in seen:
                        continue
                    seen.add(name)
                    results.append((name, _return_hint(child), child.is_const_method()))
                return
        for child in cursor.get_children():
            walk(child)

    walk(_parse(header).cursor)
    return results


# ---------------------------------------------------------------------------
# Stub writers
# ---------------------------------------------------------------------------

def _write(out: Path, content: str) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(content)
    print(f"  wrote {out.relative_to(STUBS_DIR.parent)}")


def write_listener_stub(out: Path, cpp_namespace: str,
                        structs: list[tuple[str, list[str] | None]]) -> None:
    lines = ["#pragma once", f"namespace {cpp_namespace} {{"]
    for name, params in structs:
        if params is None:
            lines.append(f"    struct {name} {{ {name}(); }};")
        else:
            lines.append(f"    struct {name} {{ {name}({', '.join(params)}); }};")
    lines += ["}", ""]
    _write(out, "\n".join(lines))


def write_class_stub(out: Path, preamble: str, class_name: str,
                     methods: list[tuple[str, str, bool]],
                     extra_decls: str = "") -> None:
    """
    Each method becomes a variadic template that covers all overloads:
        template <typename... A> double get_latest(A...) const;
    extra_decls is injected verbatim for signatures that need specific types
    (e.g. add_listeners needs std::initializer_list<ListenerConfig>).
    """
    lines = ["#pragma once", preamble, f"namespace vulcan {{", f"    class {class_name} {{",
             "    public:"]
    for name, ret, is_const in methods:
        const = " const" if is_const else ""
        lines.append(f"        template <typename... A> {ret} {name}(A...){const};")
    if extra_decls:
        for line in extra_decls.strip().splitlines():
            lines.append(f"        {line}")
    lines += ["    };", "}", ""]
    _write(out, "\n".join(lines))


# ---------------------------------------------------------------------------
# Static stubs (structure never changes with listener additions)
# ---------------------------------------------------------------------------

STUB_VULCAN_H = """\
#pragma once
#include "vulcan/feature.hpp"
#include "vulcan/listeners/global.hpp"
#include "vulcan/listeners/object.hpp"
#include "vulcan/listeners.hpp"
#include "vulcan/feature_registry.hpp"
#include "vulcan/feature_store.hpp"
#include "vulcan/rank.hpp"
"""

STUB_FEATURE_HPP = """\
#pragma once
using int64_t = long long;
namespace vulcan {
    template <typename T>
    struct feature_handle { int id; };
}
"""

STUB_LISTENERS_HPP = """\
#pragma once
#include "vulcan/listeners/global.hpp"
#include "vulcan/listeners/object.hpp"
#include <initializer_list>
namespace vulcan {
    struct ListenerConfig {
        template <typename T> ListenerConfig(T) {}
    };
}
"""

STUB_FEATURE_REGISTRY_HPP = """\
#pragma once
#include "vulcan/feature.hpp"
namespace vulcan {
    class feature_registry {
    public:
        feature_registry();
        struct GlobalFeatures {
            feature_handle<double>    declare_f64();
            feature_handle<long long> declare_i64();
        } global;
        struct ObjectFeatures {
            feature_handle<double>    declare_f64();
            feature_handle<long long> declare_i64();
        } object;
    };
}
"""

STUB_RANK_EXTRA = """\
namespace vulcan {
    namespace rank {
        enum Mechanism { FullSort, SampleSort };
    }
    inline bool min(double a, double b) { return a < b; }
    inline bool max(double a, double b) { return a > b; }
}
"""

# add_listeners must accept an initializer_list so {Listener(...), ...} works.
RANK_CONFIG_EXTRA_DECLS = """\
template <typename T>
void add_listeners(feature_handle<T> h, std::initializer_list<ListenerConfig> ls);
"""


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    print("Generating stubs...")

    # Listener stubs — auto-generated
    for scope in ("global", "object"):
        real = INCLUDE_DIR / "vulcan" / "listeners" / f"{scope}.hpp"
        ns   = f"vulcan::listeners::{scope}"
        write_listener_stub(STUBS_DIR / "vulcan" / "listeners" / f"{scope}.hpp",
                            ns, extract_listeners(real, ns))

    # feature_store — auto-generated
    fs_methods = extract_public_methods(
        INCLUDE_DIR / "vulcan" / "feature_store.hpp", "feature_store")
    write_class_stub(
        STUBS_DIR / "vulcan" / "feature_store.hpp",
        '#include "vulcan/feature.hpp"',
        "feature_store", fs_methods,
    )

    # rank_config — auto-generated (add_listeners kept explicit)
    rc_methods = [m for m in extract_public_methods(
        INCLUDE_DIR / "vulcan" / "rank.hpp", "rank_config")
        if m[0] != "add_listeners"]
    rank_preamble = (
        '#include "vulcan/feature_store.hpp"\n'
        '#include "vulcan/feature_registry.hpp"\n'
        '#include "vulcan/listeners.hpp"\n'
        '#include <initializer_list>\n'
        + STUB_RANK_EXTRA
    )
    write_class_stub(
        STUBS_DIR / "vulcan" / "rank.hpp",
        rank_preamble, "rank_config", rc_methods,
        extra_decls=RANK_CONFIG_EXTRA_DECLS,
    )

    # Static stubs
    _write(STUBS_DIR / "vulcan.h",                        STUB_VULCAN_H)
    _write(STUBS_DIR / "vulcan" / "feature.hpp",          STUB_FEATURE_HPP)
    _write(STUBS_DIR / "vulcan" / "listeners.hpp",        STUB_LISTENERS_HPP)
    _write(STUBS_DIR / "vulcan" / "feature_registry.hpp", STUB_FEATURE_REGISTRY_HPP)

    print("Done.")


if __name__ == "__main__":
    main()
