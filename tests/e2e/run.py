#!/usr/bin/env python3

import argparse
import difflib
import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = Path(__file__).resolve().parent / "sources"
EXPECTED_DIR = Path(__file__).resolve().parent / "expected"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run source-to-interpreter end-to-end snapshot tests."
    )
    parser.add_argument(
        "cases",
        nargs="*",
        help="case names to run, with or without the .txt suffix (default: all)",
    )
    parser.add_argument(
        "--update",
        action="store_true",
        help="replace expected stdout, stderr, and status snapshots for selected cases",
    )
    parser.add_argument(
        "--compiler",
        type=Path,
        default=ROOT / "ifj26",
        help="compiler executable (default: repository ifj26)",
    )
    parser.add_argument(
        "--interpreter",
        type=Path,
        default=ROOT / "tests" / "bin" / "ic26int",
        help="IFJcode26 interpreter (default: tests/bin/ic26int)",
    )
    return parser.parse_args()


def select_sources(requested: list[str]) -> list[Path]:
    sources = sorted(SOURCE_DIR.glob("*.txt"))
    if not requested:
        return sources

    selected = []
    by_stem = {source.stem: source for source in sources}
    for name in requested:
        stem = Path(name).stem
        if stem not in by_stem:
            raise ValueError(f"unknown E2E case: {name}")
        selected.append(by_stem[stem])
    return selected


def text_output(output: bytes) -> str:
    return output.decode("utf-8")


def run_case(source: Path, compiler: Path, interpreter: Path) -> dict[str, object]:
    source_code = source.read_bytes()
    compiled = subprocess.run(
        [str(compiler.resolve())],
        input=source_code,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )

    if compiled.returncode != 0:
        return {
            "compiler": {
                "status": compiled.returncode,
                "stderr": text_output(compiled.stderr),
            },
            "interpreter": None,
        }

    stdin_path = source.with_suffix(".stdin")
    program_input = stdin_path.read_bytes() if stdin_path.exists() else b""
    with tempfile.NamedTemporaryFile(suffix=".ijc") as program:
        program.write(compiled.stdout)
        program.flush()
        executed = subprocess.run(
            [str(interpreter.resolve()), "-s", program.name],
            input=program_input,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )

    return {
        "compiler": {
            "status": compiled.returncode,
            "stderr": text_output(compiled.stderr),
        },
        "interpreter": {
            "status": executed.returncode,
            "stdout": text_output(executed.stdout),
            "stderr": text_output(executed.stderr),
        },
    }


def snapshot_path(case: Path) -> Path:
    return EXPECTED_DIR / f"{case.stem}.json"


def snapshot_bytes(actual: dict[str, object]) -> bytes:
    return (json.dumps(actual, indent=2, ensure_ascii=False) + "\n").encode()


def update_snapshot(case: Path, actual: dict[str, object]) -> None:
    EXPECTED_DIR.mkdir(parents=True, exist_ok=True)
    target = snapshot_path(case)
    for snapshot in EXPECTED_DIR.glob(f"{case.stem}.*"):
        if snapshot.is_file() and snapshot != target:
            snapshot.unlink()
    target.write_bytes(snapshot_bytes(actual))


def print_diff(case: Path, expected: bytes, actual: bytes) -> None:
    expected_lines = expected.decode(errors="replace").splitlines(keepends=True)
    actual_lines = actual.decode(errors="replace").splitlines(keepends=True)
    diff = difflib.unified_diff(
        expected_lines,
        actual_lines,
        fromfile=f"expected/{case.stem}.json",
        tofile=f"actual/{case.stem}.json",
    )
    print("".join(diff), end="")


def compare_snapshot(case: Path, actual: dict[str, object]) -> bool:
    expected_path = snapshot_path(case)
    if not expected_path.exists():
        print(f"FAIL  {case.name}: missing {expected_path.relative_to(ROOT)}")
        return False

    expected = expected_path.read_bytes()
    content = snapshot_bytes(actual)
    if expected == content:
        return True

    print(f"FAIL  {case.name}: results changed")
    print_diff(case, expected, content)
    return False


def main() -> int:
    args = parse_args()
    try:
        sources = select_sources(args.cases)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 2

    for executable, label in (
        (args.compiler, "compiler"),
        (args.interpreter, "interpreter"),
    ):
        if not executable.is_file():
            print(f"missing {label}: {executable}", file=sys.stderr)
            return 2

    failures = 0
    for source in sources:
        actual = run_case(source, args.compiler, args.interpreter)
        if args.update:
            update_snapshot(source, actual)
            print(f"UPDATED  {source.name}")
        elif compare_snapshot(source, actual):
            print(f"PASS     {source.name}")
        else:
            failures += 1

    if failures:
        print(f"\n{failures} E2E case(s) failed")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
