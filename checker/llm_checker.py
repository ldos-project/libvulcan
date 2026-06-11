"""
LLM Checker — verifies the properties of the LLM-generated code segment
in Vulcan.

Rules for the EVOLVE-BLOCK:
1. No new/delete or free/malloc
2. No std:: library use at all (must use vulcan:: namespaces)
3. Top-level statements MUST ONLY be config method calls or declaring a scoring function
4. No arrays
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
import clang.cindex as cx

ROOT           = Path(__file__).parent.parent
ALLOWED_PREFIX = "vulcan::"
INCLUDE_DIR    = ROOT / "stubs"

# We must set correct libclang path to match the installed clang package
cx.Config.set_library_file('/usr/lib/llvm-18/lib/libclang.so')

def fully_qualified(cursor: cx.Cursor) -> str:
    parts: list[str] = []
    cur = cursor
    while cur and cur.kind not in (
        cx.CursorKind.TRANSLATION_UNIT,
        cx.CursorKind.INVALID_FILE,
    ):
        name = cur.spelling or ""
        if name:
            parts.append(name)
        parent = cur.semantic_parent
        if parent is None or parent == cur:
            break
        cur = parent
    return "::".join(reversed(parts))


def name_is_allowed(fqn: str) -> bool:
    if not fqn:
        return True
    if "(lambda" in fqn:
        return True
    return fqn.startswith(ALLOWED_PREFIX)


class Violation:
    __slots__ = ("file", "line", "col", "rule", "message")

    def __init__(
        self, file: str, line: int, col: int, rule: str, message: str
    ) -> None:
        self.file = file
        self.line = line
        self.col = col
        self.rule = rule
        self.message = message

    def __str__(self) -> str:
        return f"{self.file}:{self.line}:{self.col}: [{self.rule}] {self.message}"


def loc(cursor: cx.Cursor) -> tuple[str, int, int]:
    loc_ = cursor.location
    return (loc_.file.name if loc_.file else "<unknown>", loc_.line, loc_.column)


CAST_KINDS = frozenset([
    cx.CursorKind.CSTYLE_CAST_EXPR,
    cx.CursorKind.CXX_REINTERPRET_CAST_EXPR,
    cx.CursorKind.CXX_CONST_CAST_EXPR,
])


class LLMChecker:
    def __init__(self, source_file: Path) -> None:
        self.source_file = source_file.resolve()
        self.violations: list[Violation] = []
        self.start_line = -1
        self.end_line = -1
        self._find_block()

    def _find_block(self) -> None:
        with open(self.source_file) as f:
            lines = f.readlines()
        for i, line in enumerate(lines):
            if "EVOLVE-BLOCK-START" in line:
                self.start_line = i + 1
            if "EVOLVE-BLOCK-END" in line:
                self.end_line = i + 1

    def _in_block(self, line: int) -> bool:
        if self.start_line == -1 or self.end_line == -1:
            return False
        return self.start_line <= line <= self.end_line

    def _check_includes(self, tu: cx.TranslationUnit) -> None:
        for inc in tu.get_includes():
            src = inc.source
            if src is None:
                continue
            
            src_path = Path(src.name).resolve()
            if src_path != self.source_file:
                continue

            inc_path = Path(inc.include.name).resolve()
            try:
                inc_path.relative_to(INCLUDE_DIR)
            except ValueError:
                self.violations.append(
                    Violation(
                        str(src_path),
                        inc.location.line,
                        inc.location.column,
                        "DISALLOWED_INCLUDE",
                        f"#include of '{inc_path}' is outside the allowed include directory '{INCLUDE_DIR}'"
                    )
                )

    def run(self) -> int:
        if self.start_line == -1 or self.end_line == -1:
            print("Warning: Could not find EVOLVE-BLOCK-START or EVOLVE-BLOCK-END. Checking whole file.", file=sys.stderr)

        index = cx.Index.create()
        args = [
            "-x", "c++",
            "-std=c++17",
            f"-I{INCLUDE_DIR}",
            f"-I{ROOT}",
            "-isystem/usr/lib/llvm-18/lib/clang/18/include",
            "-ferror-limit=0",
        ]
        tu = index.parse(
            str(self.source_file),
            args=args,
            options=cx.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD,
        )

        self._check_includes(tu)

        for diag in tu.diagnostics:
            if diag.severity >= cx.Diagnostic.Error:
                if diag.location.file and Path(diag.location.file.name).resolve() == self.source_file:
                    self.violations.append(Violation(diag.location.file.name, diag.location.line, diag.location.column, "PARSE_ERROR", diag.spelling))

        self._walk(tu.cursor)

        for v in self.violations:
            print(v, file=sys.stderr)

        return min(len(self.violations), 127)

    def _is_main_body(self, cursor: cx.Cursor) -> bool:
        """True only if cursor is the COMPOUND_STMT of the top-level function (e.g. main)."""
        if cursor.kind != cx.CursorKind.COMPOUND_STMT:
            return False
        parent = cursor.semantic_parent
        if not parent or parent.kind != cx.CursorKind.FUNCTION_DECL:
            return False
        grandparent = parent.semantic_parent
        return grandparent is not None and grandparent.kind == cx.CursorKind.TRANSLATION_UNIT

    def _walk(self, cursor: cx.Cursor) -> None:
        if cursor.location.file and Path(cursor.location.file.name).resolve() != self.source_file:
            return

        is_main_body = self._is_main_body(cursor)

        for child in cursor.get_children():
            self._check_node(child, is_top_level_stmt=is_main_body)
            self._walk(child)

    def _check_node(self, cursor: cx.Cursor, is_top_level_stmt: bool) -> None:
        if not cursor.location.file:
            return
        
        kind = cursor.kind

        # Rule 3: Top-level statements inside the evolve block
        # Only enforced if we are inside the block
        if is_top_level_stmt and self.start_line != -1 and self.end_line != -1 and self._in_block(cursor.location.line):
            allowed_top_level = False
            if kind == cx.CursorKind.DECL_STMT:
                allowed_top_level = True
            else:
                def has_config(cur: cx.Cursor) -> bool:
                    if cur.kind == cx.CursorKind.DECL_REF_EXPR and cur.spelling == "config":
                        return True
                    for c in cur.get_children():
                        if has_config(c):
                            return True
                    return False
                if kind in (cx.CursorKind.CALL_EXPR, cx.CursorKind.UNEXPOSED_EXPR) and has_config(cursor):
                    allowed_top_level = True
            
            if not allowed_top_level:
                f, l, c = loc(cursor)
                self.violations.append(
                    Violation(f, l, c, "INVALID_TOP_LEVEL_STMT",
                              "Top-level statements in EVOLVE-BLOCK must be config methods or declarations")
                )

        # Rule 2: No std:: library, only vulcan::
        if kind == cx.CursorKind.CALL_EXPR:
            ref = cursor.referenced
            fqn = fully_qualified(ref) if (ref and ref.kind != cx.CursorKind.NO_DECL_FOUND) else (cursor.spelling or "")
            if fqn and not name_is_allowed(fqn):
                f, l, c = loc(cursor)
                self.violations.append(
                    Violation(f, l, c, "DISALLOWED_CALL",
                              f"Call to '{fqn}' is outside the allowed namespace '{ALLOWED_PREFIX}'")
                )

        # Rule 1: No new/delete
        elif kind in (cx.CursorKind.CXX_NEW_EXPR, cx.CursorKind.CXX_DELETE_EXPR):
            label = "new" if kind == cx.CursorKind.CXX_NEW_EXPR else "delete"
            f, l, c = loc(cursor)
            self.violations.append(
                Violation(f, l, c, "RAW_MEMORY_EXPR", f"'{label}' expression is disallowed")
            )

        # Rule 4: No Arrays
        elif kind == cx.CursorKind.ARRAY_SUBSCRIPT_EXPR:
            f, l, c = loc(cursor)
            self.violations.append(
                Violation(f, l, c, "ARRAY_SUBSCRIPT", "Array subscript operator is disallowed")
            )

        elif kind in CAST_KINDS:
            cast_name = {
                cx.CursorKind.CSTYLE_CAST_EXPR: "C-style cast",
                cx.CursorKind.CXX_REINTERPRET_CAST_EXPR: "reinterpret_cast",
                cx.CursorKind.CXX_CONST_CAST_EXPR: "const_cast",
            }[kind]
            f, l, c = loc(cursor)
            self.violations.append(
                Violation(f, l, c, "UNSAFE_CAST", f"'{cast_name}' is disallowed")
            )

        elif kind == cx.CursorKind.ASM_STMT:
            f, l, c = loc(cursor)
            self.violations.append(
                Violation(f, l, c, "INLINE_ASM", "Inline assembly is disallowed")
            )

        elif kind == cx.CursorKind.CXX_THROW_EXPR:
            f, l, c = loc(cursor)
            self.violations.append(
                Violation(f, l, c, "THROW_EXPR", "'throw' statement is disallowed")
            )

        elif kind in (cx.CursorKind.GOTO_STMT, cx.CursorKind.LABEL_STMT):
            label = "goto" if kind == cx.CursorKind.GOTO_STMT else "label"
            f, l, c = loc(cursor)
            self.violations.append(
                Violation(f, l, c, "GOTO", f"'{label}' is disallowed")
            )

        elif kind in (cx.CursorKind.WHILE_STMT, cx.CursorKind.DO_STMT):
            label = "while" if kind == cx.CursorKind.WHILE_STMT else "do-while"
            f, l, c = loc(cursor)
            self.violations.append(
                Violation(f, l, c, "BANNED_LOOP", f"'{label}' loops are disallowed; use bounded 'for' loops instead")
            )

        elif kind == cx.CursorKind.FOR_STMT:
            self._check_for_loop_bounds(cursor)

        elif kind == cx.CursorKind.UNARY_OPERATOR:
            tokens = list(cursor.get_tokens())
            if tokens:
                if tokens[0].spelling == "&":
                    f, l, c = loc(cursor)
                    self.violations.append(
                        Violation(f, l, c, "ADDRESS_OF", "Address-of operator '&' is disallowed")
                    )
                elif tokens[0].spelling == "*":
                    f, l, c = loc(cursor)
                    self.violations.append(
                        Violation(f, l, c, "DEREFERENCE", "Dereference operator '*' is disallowed")
                    )

        elif kind == cx.CursorKind.FUNCTION_DECL and self.start_line != -1 and self.end_line != -1 and self._in_block(cursor.location.line):
            f, l, c = loc(cursor)
            self.violations.append(
                Violation(f, l, c, "FUNCTION_DECL", "Declaring external functions inside EVOLVE-BLOCK is disallowed")
            )

        elif kind in (cx.CursorKind.VAR_DECL, cx.CursorKind.FIELD_DECL, cx.CursorKind.PARM_DECL):
            typ = cursor.type.get_canonical()
            if typ.kind == cx.TypeKind.POINTER:
                f, l, c = loc(cursor)
                self.violations.append(
                    Violation(f, l, c, "POINTER_VAR", "Variable, field, or parameter has pointer type, which is disallowed")
                )
            elif typ.kind in (cx.TypeKind.CONSTANTARRAY, cx.TypeKind.INCOMPLETEARRAY, cx.TypeKind.VARIABLEARRAY, cx.TypeKind.DEPENDENTSIZEDARRAY):
                f, l, c = loc(cursor)
                self.violations.append(
                    Violation(f, l, c, "ARRAY_VAR", "Variable, field, or parameter has array type, which is disallowed")
                )

    def _check_for_loop_bounds(self, cursor: cx.Cursor) -> None:
        """Require for-loops to have integer literal init and bound."""
        children = list(cursor.get_children())
        # A well-formed for(init; cond; incr) body has 4 children
        # but clang may omit missing parts, so we check what's there
        f, l, c = loc(cursor)

        # Find the init (DECL_STMT or first child) and condition
        init_ok = False
        cond_ok = False

        for child in children:
            if child.kind == cx.CursorKind.DECL_STMT:
                # Check that the variable is initialized with an integer literal
                for var in child.get_children():
                    if var.kind == cx.CursorKind.VAR_DECL:
                        for init_expr in var.get_children():
                            if init_expr.kind == cx.CursorKind.INTEGER_LITERAL:
                                init_ok = True
            elif child.kind == cx.CursorKind.BINARY_OPERATOR:
                # Condition: one side must be an integer literal
                def has_int_literal(cur: cx.Cursor) -> bool:
                    if cur.kind == cx.CursorKind.INTEGER_LITERAL:
                        return True
                    for ch in cur.get_children():
                        if ch.kind == cx.CursorKind.INTEGER_LITERAL:
                            return True
                    return False
                if has_int_literal(child):
                    cond_ok = True

        if not init_ok or not cond_ok:
            self.violations.append(
                Violation(f, l, c, "UNBOUNDED_FOR_LOOP",
                          "'for' loops must have constexpr integer literal init and bound")
            )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", help="Vulcan-generated C++ source file to check")
    args = parser.parse_args()

    source = Path(args.source)
    if not source.exists():
        print(f"error: file not found: {source}", file=sys.stderr)
        sys.exit(1)

    checker = LLMChecker(source)
    sys.exit(checker.run())


if __name__ == "__main__":
    main()
