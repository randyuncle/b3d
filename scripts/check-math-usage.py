#!/usr/bin/env python3
"""
Check that examples use b3d-math.h wrappers instead of raw <math.h> functions.

Examples should use:
  b3d_sinf(), b3d_cosf(), b3d_tanf(), b3d_sqrtf(), b3d_fabsf(), b3d_sincosf()

Instead of:
  sinf(), cosf(), tanf(), sqrtf(), fabsf(), sincosf()
"""

import re
import sys
from pathlib import Path

# Raw math.h functions that should be replaced with b3d_* wrappers
FORBIDDEN_FUNCS = [
    "sinf",
    "cosf",
    "tanf",
    "sqrtf",
    "fabsf",
    "sincosf",
]

# Pattern to match function calls (not prefixed by b3d_)
# Matches: sinf( but not b3d_sinf( or _sinf( or asinf(
FUNC_PATTERN = re.compile(r"(?<![a-zA-Z0-9_])(" + "|".join(FORBIDDEN_FUNCS) + r")\s*\(")

# Pattern to check for b3d-math.h include
INCLUDE_PATTERN = re.compile(r'#include\s+[<"]b3d-math\.h[>"]')


def strip_comments_and_strings(content: str) -> list[str]:
    """
    Strip C comments and string literals from content, preserving line structure.

    Returns a list of lines with comments and strings replaced by spaces.
    """
    result = []
    lines = content.splitlines()
    in_block_comment = False

    for line in lines:
        cleaned = []
        i = 0
        n = len(line)

        while i < n:
            if in_block_comment:
                # Look for end of block comment
                end = line.find("*/", i)
                if end != -1:
                    cleaned.append(" " * (end - i + 2))
                    i = end + 2
                    in_block_comment = False
                else:
                    cleaned.append(" " * (n - i))
                    break
            elif line[i : i + 2] == "/*":
                # Start of block comment
                end = line.find("*/", i + 2)
                if end != -1:
                    cleaned.append(" " * (end - i + 2))
                    i = end + 2
                else:
                    cleaned.append(" " * (n - i))
                    in_block_comment = True
                    break
            elif line[i : i + 2] == "//":
                # Line comment - rest of line is comment
                cleaned.append(" " * (n - i))
                break
            elif line[i] == '"':
                # String literal
                cleaned.append(" ")
                i += 1
                while i < n:
                    if line[i] == "\\" and i + 1 < n:
                        cleaned.append("  ")
                        i += 2
                    elif line[i] == '"':
                        cleaned.append(" ")
                        i += 1
                        break
                    else:
                        cleaned.append(" ")
                        i += 1
            elif line[i] == "'":
                # Character literal
                cleaned.append(" ")
                i += 1
                while i < n:
                    if line[i] == "\\" and i + 1 < n:
                        cleaned.append("  ")
                        i += 2
                    elif line[i] == "'":
                        cleaned.append(" ")
                        i += 1
                        break
                    else:
                        cleaned.append(" ")
                        i += 1
            else:
                cleaned.append(line[i])
                i += 1

        result.append("".join(cleaned))

    return result


def check_file(filepath: Path) -> list[tuple[int, str, str]]:
    """
    Check a single file for forbidden math.h function usage.

    Returns list of (line_number, function_name, line_content) tuples.
    """
    errors = []
    try:
        content = filepath.read_text()
    except (OSError, UnicodeDecodeError) as e:
        print(f"Warning: Cannot read {filepath}: {e}", file=sys.stderr)
        return errors

    has_b3d_math_include = bool(INCLUDE_PATTERN.search(content))
    uses_b3d_wrappers = bool(
        re.search(r"\bb3d_(sinf|cosf|tanf|sqrtf|fabsf|sincosf)\s*\(", content)
    )

    # Strip comments and strings for checking
    original_lines = content.splitlines()
    cleaned_lines = strip_comments_and_strings(content)

    # Check each line for forbidden functions
    for lineno, (orig, cleaned) in enumerate(
        zip(original_lines, cleaned_lines), start=1
    ):
        # Find forbidden function calls in cleaned line
        for match in FUNC_PATTERN.finditer(cleaned):
            func_name = match.group(1)
            errors.append((lineno, func_name, orig.strip()))

    # Only require b3d-math.h if file uses b3d_* wrappers but forgot the include
    if uses_b3d_wrappers and not has_b3d_math_include:
        errors.insert(0, (0, "missing-include", "b3d-math.h not included"))

    return errors


def main() -> int:
    # Determine examples directory
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    examples_dir = project_root / "examples"

    if not examples_dir.is_dir():
        print(f"Error: Examples directory not found: {examples_dir}", file=sys.stderr)
        return 1

    # Find all .c files in examples/
    c_files = sorted(examples_dir.glob("*.c"))
    if not c_files:
        print(f"Warning: No .c files found in {examples_dir}", file=sys.stderr)
        return 0

    total_errors = 0
    files_with_errors = 0

    for filepath in c_files:
        errors = check_file(filepath)
        if errors:
            files_with_errors += 1
            print(f"\n{filepath.relative_to(project_root)}:")
            for lineno, func, line in errors:
                if func == "missing-include":
                    print(f"  error: {line}")
                else:
                    print(f"  {lineno}: use b3d_{func}() instead of {func}()")
                    print(f"       {line}")
                total_errors += 1

    # Summary
    if total_errors > 0:
        print(f"\n{total_errors} error(s) in {files_with_errors} file(s)")
        print("Use b3d-math.h wrappers instead of raw <math.h> functions:")
        print("  sinf()    -> b3d_sinf()")
        print("  cosf()    -> b3d_cosf()")
        print("  tanf()    -> b3d_tanf()")
        print("  sqrtf()   -> b3d_sqrtf()")
        print("  fabsf()   -> b3d_fabsf()")
        print("  sincosf() -> b3d_sincosf()")
        return 1

    print(f"OK: {len(c_files)} example(s) checked, all using b3d-math.h")
    return 0


if __name__ == "__main__":
    sys.exit(main())
