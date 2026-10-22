#!/usr/bin/env python3
"""T4 独立回归：临时构建、两种 PMM/内存容量及逐次启动的预期 panic。"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-riscv64"))
    parser.add_argument("--log-dir", type=Path, default=Path("slub-test-logs"))
    args = parser.parse_args()
    qemu = shutil.which(args.qemu)
    if not qemu:
        parser.error("找不到 QEMU；请通过 --qemu 指定统一的 4.1.1 版本")
    version = subprocess.check_output([qemu, "--version"], text=True)
    if not version.startswith("QEMU emulator version 4.1.1\n"):
        parser.error("需要 QEMU 4.1.1（自带 OpenSBI v0.4）")
    args.log_dir.mkdir(parents=True, exist_ok=True)
    (args.log_dir / "environment.log").write_text(version, encoding="utf-8")
    source = Path(__file__).resolve().parents[1]
    expected = {
        1: r"slub: double free of .* in cache kmalloc-32",
        2: r"slub: .* does not belong to cache kmalloc-64",
        3: r"slub: .* is not an object boundary in cache kmalloc-32",
        4: r"kfree: invalid pointer .*",
        5: r"kfree: invalid pointer .*",
        6: r"kfree: invalid pointer .*",
    }
    cases = [(pmm, memory, 0) for pmm in ("best_fit", "buddy") for memory in (128, 64)]
    cases += [("best_fit", 128, case) for case in expected]
    results = []
    with tempfile.TemporaryDirectory(prefix="lyp-t4-slub-") as tmp:
        work = Path(tmp) / "code"
        shutil.copytree(source, work, ignore=shutil.ignore_patterns(
            "obj", "bin", "__pycache__", "slub-test-logs", ".gdb.in", ".qemu.out"))
        built = None
        for pmm, memory, case in cases:
            name = f"{pmm}-{memory}M-case{case}"
            if built != (pmm, case):
                # DEFS 不在框架的依赖跟踪范围内，每个条件编译变体都重新构建。
                subprocess.run(["make", "clean"], cwd=work, check=True, stdout=subprocess.DEVNULL)
                build = subprocess.run(["make", "-j4", f"PMM={pmm}",
                    f"DEFS=-DSLUB_PANIC_TEST={case}"], cwd=work,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                (args.log_dir / f"{name}-build.log").write_text(build.stdout, encoding="utf-8")
                if build.returncode:
                    raise RuntimeError(f"{name} 编译失败：\n{build.stdout}")
                built = (pmm, case)
            command = [qemu, "-machine", "virt", "-nographic", "-bios", "default",
                "-m", str(memory), "-device", "loader,file=bin/ucore.img,addr=0x80200000"]
            try:
                run = subprocess.run(command, cwd=work, stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, timeout=6)
                output = run.stdout.decode("utf-8", errors="replace")
                # 正常内核和 panic 均停在循环中，提前退出不是成功。
                raise RuntimeError(f"{name}: QEMU 提前退出 ({run.returncode})\n{output}")
            except subprocess.TimeoutExpired as exc:
                output = (exc.stdout or b"").decode("utf-8", errors="replace")
            (args.log_dir / f"{name}.log").write_text(output, encoding="utf-8")
            assert "OpenSBI v0.4" in output, name
            assert f"memory management: {pmm}_pmm_manager" in output, name
            if case:
                assert f"[SLUB panic {case}] begin" in output, name
                assert re.search(r"kernel panic at kern/mm/slub\.c:\d+:\s+" + expected[case], output), name
                assert output.count("kernel panic at") == 1, name
                assert "unexpectedly returned" not in output, name
            else:
                assert "panic" not in output.lower(), name
                assert "[SLUB] ALL TESTS PASSED" in output, name
                assert re.search(r"round 1: PASS .*retained 7\)", output), name
                assert re.search(r"round 2: PASS .*retained 0\)", output), name
            result = f"{name}: PASS" + (" (expected panic)" if case else "")
            results.append(result)
            print(result, flush=True)
    (args.log_dir / "summary.log").write_text("\n".join(results) + "\n", encoding="utf-8")
    print("T4: 4 normal boots + 6 isolated panic cases passed", flush=True)


if __name__ == "__main__":
    main()
