#!/usr/bin/env python3
"""
C/C++ 静态检查脚本
- 跳过 .gitignore、tests、examples 全部内容
- 命名空间任意位置含 Literals（不区分大小写）即放行
- 只在纯代码区检测 new/malloc，字符串/注释/胶水 .new 已剔除
"""

import os
import re
import sys
import argparse
import pathspec
from pathlib import Path
from typing import List, Dict

# ---------- gitignore ----------
class GitignoreMatcher:
    def __init__(self, root: Path):
        self.root = root.resolve()
        self.spec = pathspec.PathSpec.from_lines("gitwildmatch", self._load_patterns())

    def _load_patterns(self) -> List[str]:
        default = [
            "build/", "cmake-build-*/", "out/", "bin/", "obj/", ".vs/", ".vscode/",
            "*.o", "*.obj", "*.exe", "*.dll", "*.so", "*.pdb", "*.ilk", ".git/", "__pycache__/",
        ]
        gitignore = self.root / ".gitignore"
        if gitignore.exists():
            with gitignore.open(encoding="utf-8") as f:
                default += [line.strip() for line in f if line.strip() and not line.startswith("#")]
        return default

    def ignored(self, path: Path) -> bool:
        try:
            relpath = path.resolve().relative_to(self.root)
            return self.spec.match_file(str(relpath))
        except ValueError:
            return False


# ---------- 内存泄漏 ----------
class SmartMemoryLeakDetector:
    def __init__(self):
        self.smart = {
            "std::unique_ptr", "std::shared_ptr", "std::weak_ptr", "std::auto_ptr",
            "unique_ptr", "shared_ptr", "weak_ptr", "auto_ptr",
            "QSharedPointer", "QWeakPointer", "QScopedPointer", "QPointer",
            "sp", "wp", "TSharedPtr", "TSharedRef", "TWeakPtr", "TUniquePtr",
            "boost::shared_ptr", "boost::weak_ptr",
        }
        self.widget_prefix = re.compile(r"^(Q|wx|U|T)[A-Z]\w*")
        self.placement = ["new (", "new[] (", "operator new", "operator delete"]

    def _whole_word(self, line: str, pos: int, word: str) -> bool:
        left = line[:pos]
        right = line[pos + len(word):]
        left_ok = not left or not (left[-1].isalnum() or left[-1] == "_")
        right_ok = not right or not (right[0].isalnum() or right[0] == "_")
        return left_ok and right_ok

    def _skip_literal_comment(self, content: str) -> str:
        out, i, n = [], 0, len(content)
        in_str = in_char = in_block = in_line = False
        while i < n:
            if in_block:
                if content[i:i+2] == "*/":
                    in_block = False
                    out.append("  ")
                    i += 2
                    continue
                out.append(" ")
                i += 1
                continue
            if in_line:
                if content[i] == "\n":
                    in_line = False
                    out.append("\n")
                    i += 1
                    continue
                out.append(" ")
                i += 1
                continue
            if in_str:
                if content[i] == '"' and content[i-1] != "\\":
                    in_str = False
                out.append(content[i])
                i += 1
                continue
            if in_char:
                if content[i] == "'" and content[i-1] != "\\":
                    in_char = False
                out.append(content[i])
                i += 1
                continue
            if content[i:i+2] == "//":
                in_line = True
                out.append("//")
                i += 2
            elif content[i:i+2] == "/*":
                in_block = True
                out.append("/*")
                i += 2
            elif content[i] == '"' and (i == 0 or content[i-1] != "\\"):
                in_str = True
                out.append('"')
                i += 1
            elif content[i] == "'" and (i == 0 or content[i-1] != "\\"):
                in_char = True
                out.append("'")
                i += 1
            else:
                out.append(content[i])
                i += 1
        return "".join(out)

    def _find_allocs(self, content: str) -> List[Dict]:
        pure = self._skip_literal_comment(content)
        lines = pure.split("\n")
        allocs = []
        for idx, raw in enumerate(lines, 1):
            line = raw.strip()
            if any(k in line for k in self.placement):
                continue
            for m in re.finditer(r"\bnew\b", line, re.IGNORECASE):
                if not self._whole_word(line, m.start(), "new"):
                    continue
                after = line[m.end():].lstrip()
                if not after:
                    continue
                if self.widget_prefix.match(after):
                    continue
                if any(ptr in line for ptr in self.smart):
                    continue
                if re.search(r"\.\s*new\s*\(", line):
                    continue
                allocs.append({"type": "new", "line": idx, "col": m.start() + 1, "content": line})
            for pat, name in [(r"\bmalloc\b", "malloc"), (r"\bcalloc\b", "calloc"), (r"\brealloc\b", "realloc")]:
                for m in re.finditer(pat, line, re.IGNORECASE):
                    if not self._whole_word(line, m.start(), name):
                        continue
                    if any(ptr in line for ptr in self.smart):
                        continue
                    allocs.append({"type": name, "line": idx, "col": m.start() + 1, "content": line})
        return allocs

    def check(self, content: str) -> List[Dict]:
        pure_lines = self._skip_literal_comment(content).split("\n")
        orig_lines = content.split("\n")
        allocs = self._find_allocs(content)
        issues = []
        for alloc in allocs:
            ty = alloc["type"]
            lnum = alloc["line"]
            found = False
            start_search = lnum - 1
            end_search = min(len(pure_lines), lnum + 20)
            for i in range(start_search, end_search):
                if i >= len(pure_lines):
                    continue
                line = pure_lines[i]
                if ty == "new":
                    for m in re.finditer(r"\bdelete\b", line, re.I):
                        if self._whole_word(line, m.start(), "delete"):
                            found = True
                            break
                elif ty in ["malloc", "calloc", "realloc"]:
                    for m in re.finditer(r"\bfree\b", line, re.I):
                        if self._whole_word(line, m.start(), "free"):
                            found = True
                            break
                if found:
                    break
            if not found and not self._has_smart_nearby(orig_lines, lnum - 1):
                issues.append(
                    {
                        "type": "potential_memory_leak",
                        "line": lnum,
                        "column": alloc["col"],
                        "message": f"{ty} 后 20 行内未找到配对释放",
                        "code": orig_lines[lnum - 1].strip(),
                    }
                )
        return issues

    def _has_smart_nearby(self, lines: List[str], line_idx: int, distance: int = 5) -> bool:
        start = max(0, line_idx - distance)
        end = min(len(lines), line_idx + distance + 1)
        for i in range(start, end):
            if any(ptr in lines[i] for ptr in self.smart):
                return True
        return False


# ---------- 主分析器 ----------
class CppStaticAnalyzer:
    def __init__(self, root: Path, complexity_threshold: int = 10):
        self.root = root
        self.threshold = complexity_threshold
        self.git = GitignoreMatcher(root)
        self.mem = SmartMemoryLeakDetector()
        self.cxx_ext = {".cpp", ".cc", ".cxx", ".c++", ".hpp", ".hh", ".hxx", ".h++", ".c", ".h"}

    def is_cpp(self, p: Path) -> bool:
        return p.suffix.lower() in self.cxx_ext

    def is_test_or_example(self, p: Path) -> bool:
        return any(part.lower() in {"tests", "examples", "test", "example"} for part in p.parts)

    def get_relative_path(self, path: Path) -> str:
        try:
            return str(path.relative_to(self.root))
        except ValueError:
            return str(path)

    def complex(self, content: str) -> int:
        base = 1
        keys = [r"\bif\s*\(", r"\belse\b", r"\bwhile\s*\(", r"\bfor\s*\(", r"\bcase\b", r"\bcatch\s*\(", r"\b&&\b", r"\b\|\|\b", r"\b\?\b"]
        for k in keys:
            base += len(re.findall(k, content, re.I))
        return base

    def analyze_file(self, filepath: Path) -> List[Dict]:
        if self.is_test_or_example(filepath):
            return []
        if self.git.ignored(filepath):
            return []
        if not self.is_cpp(filepath):
            return []
        try:
            content = filepath.read_text(encoding="utf-8", errors="ignore")
        except Exception as e:
            return [{"type": "file_error", "line": 0, "column": 0, "message": f"无法读取文件: {str(e)}", "code": ""}]
        issues = []

        # 1. catch(...)
        for m in re.finditer(r"\bcatch\s*\(\s*\.\.\.\s*\)", content, re.I):
            line = content[: m.start()].count("\n") + 1
            issues.append({"type": "catch_all_exception", "line": line, "column": m.start() - content.rfind("\n", 0, m.start()), "message": "禁止 catch(...)", "code": m.group().strip()})

        # 2. using namespace（任意位置含 Literals 即放行）
        for m in re.finditer(r"\busing\s+namespace\s+([\w:]+)\b", content, re.I):
            ns_name = m.group(1)
            # ← 关键修正：任意位置出现 Literals 整词即跳过
            if re.search(r"Literals", ns_name, re.I):
                continue
            line = content[: m.start()].count("\n") + 1
            issues.append({"type": "using_namespace", "line": line, "column": m.start() - content.rfind("\n", 0, m.start()), "message": f"禁止 using namespace {ns_name}", "code": m.group().strip()})

        # 3. 不安全函数
        for func in ["gets", "strcpy", "strcat", "sprintf", "vsprintf"]:
            for m in re.finditer(rf"\b{func}\s*\(", content, re.I):
                line = content[: m.start()].count("\n") + 1
                issues.append({"type": "unsafe_function", "line": line, "column": m.start() - content.rfind("\n", 0, m.start()), "message": f"避免使用 {func}", "code": m.group().strip()})

        # 4. 魔法数字
        for m in re.finditer(r"\b\d{2,}\b", content):
            line_start = content.rfind("\n", 0, m.start()) + 1
            line_end = content.find("\n", m.start())
            if line_end == -1:
                line_end = len(content)
            line_content = content[line_start:line_end].strip()
            if any(k in line_content.lower() for k in ["const", "#define", "sizeof", "array", "[]"]):
                continue
            if "//" in line_content and line_content.find("//") < line_content.find(m.group()):
                continue
            if "/*" in line_content and line_content.find("/*") < line_content.find(m.group()):
                continue
            line = content[: m.start()].count("\n") + 1
            issues.append({"type": "magic_number", "line": line, "column": m.start() - line_start + 1, "message": "避免魔法数字", "code": m.group().strip()})

        # 5. 内存泄漏（已剔除字符串/注释/胶水 .new）
        issues.extend(self.mem.check(content))

        # 6. 圈复杂度
        func_pat = re.compile(r"(\w+\s+\w+\s*\([^)]*\)\s*(?:const\s*)?(?:noexcept\s*)?\{)", re.I)
        for m in func_pat.finditer(content):
            func_body, ok = self._extract_braces(content, m.end() - 1)
            if not ok:
                continue
            comp = self.complex(func_body)
            if comp > self.threshold:
                name = re.search(r"(\w+)\s*\(", m.group(1)).group(1)
                line = content[: m.start()].count("\n") + 1
                issues.append({"type": "high_complexity", "line": line, "column": 1, "message": f"函数 {name} 圈复杂度 {comp}", "code": m.group(1)[:60] + "..."})

        # 统一相对路径
        relative_path = self.get_relative_path(filepath)
        for issue in issues:
            issue["file"] = relative_path

        return issues

    def _extract_braces(self, content: str, start: int) -> tuple:
        brace = 0
        i = start
        in_str = in_char = in_block = in_line = False
        while i < len(content):
            if not in_block and not in_line:
                if content[i] == '"' and (i == 0 or content[i-1] != "\\"):
                    in_str = not in_str
                elif content[i] == "'" and (i == 0 or content[i-1] != "\\"):
                    in_char = not in_char
                elif not in_str and not in_char:
                    if content[i:i+2] == "/*":
                        in_block = True
                        i += 1
                    elif content[i:i+2] == "//":
                        in_line = True
                        i += 1
                    elif content[i] == "{":
                        brace += 1
                    elif content[i] == "}":
                        brace -= 1
                        if brace == 0:
                            return content[start:i+1], True
                else:
                    if in_block and content[i:i+2] == "*/":
                        in_block = False
                        i += 1
                    elif in_line and content[i] == "\n":
                        in_line = False
            i += 1
        return "", False

    def run(self, path: Path) -> List[Dict]:
        if path.is_file():
            return self.analyze_file(path)
        results = []
        for p in path.rglob("*"):
            if p.is_file() and not self.git.ignored(p):
                results.extend(self.analyze_file(p))
        return results


# ---------- CLI ----------
def main():
    parser = argparse.ArgumentParser(description="C/C++ 静态检查（跳过 tests/examples，命名空间含 Literals 放行，相对路径）")
    parser.add_argument("path", help="文件或目录")
    parser.add_argument("-c", "--complexity-threshold", type=int, default=10, help="圈复杂度阈值")
    parser.add_argument("-o", "--output", help="输出报告文件")
    args = parser.parse_args()

    root = Path(args.path).resolve()
    if not root.exists():
        print(f"错误: {root} 不存在")
        sys.exit(1)

    analyzer = CppStaticAnalyzer(root.parent if root.is_file() else root, args.complexity_threshold)
    issues = analyzer.run(root)

    if not issues:
        report = "✅ 未发现问题，代码质量良好！"
    else:
        report = ["📊 静态代码分析报告\n" + "=" * 50]
        files = {}
        for iss in issues:
            files.setdefault(iss.get("file", "unknown"), []).append(iss)
        for fp, lst in files.items():
            report.append(f"\n📁 {fp}\n" + "-" * 40)
            for iss in lst:
                icon = {"catch_all_exception": "⚠️", "using_namespace": "⚠️", "unsafe_function": "🚫", "magic_number": "🔢", "potential_memory_leak": "💧", "high_complexity": "🌀", "file_error": "❌"}.get(iss["type"], "⚠️")
                report.append(f"  {icon} {iss['message']}\n     位置: 第{iss['line']}行 第{iss['column']}列\n     代码: {iss['code']}")
        report = "\n".join(report)

    if args.output:
        Path(args.output).write_text(report, encoding="utf-8")
        print(f"报告已保存到 {args.output}")
    else:
        print(report)

    if any(i["type"] in {"catch_all_exception", "unsafe_function", "file_error"} for i in issues):
        sys.exit(1)


if __name__ == "__main__":
    main()