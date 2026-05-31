#!/usr/bin/env python3
"""
clang-format-batch.py
一键格式化项目中的 C/C++ 文件，自动排除 .gitignore 中的目录和文件。
"""

import os
import sys
import argparse
import subprocess
import fnmatch
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import List, Set, Tuple


# ============ C/C++ 文件扩展名 ============
CPP_EXTENSIONS = {
    '.c', '.cpp', '.cc', '.cxx', '.c++', '.m', '.mm',
    '.h', '.hpp', '.hh', '.hxx', '.h++', '.inl', '.inc', '.ipp', '.tpp'
}


# ============ .gitignore 解析器 ============
class GitIgnoreParser:
    def __init__(self, repo_root: Path):
        self.repo_root = repo_root.resolve()
        self.rules: List[Tuple[bool, str, bool]] = []  # (is_negation, pattern, is_dir_only)
        self.has_gitignore = False
        self._load()

    def _load(self):
        gitignore_path = self.repo_root / '.gitignore'
        if not gitignore_path.exists():
            return

        self.has_gitignore = True
        with open(gitignore_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                line = line.rstrip('\n\r')
                # 跳过空行和注释
                if not line or line.startswith('#'):
                    continue
                
                is_negation = line.startswith('!')
                if is_negation:
                    line = line[1:]
                
                is_dir_only = line.endswith('/')
                if is_dir_only:
                    line = line[:-1]
                
                # 去除前导空格（gitignore 不允许前导空格，除非是转义）
                if line.startswith('\\'):
                    line = line[1:]
                
                if line:
                    self.rules.append((is_negation, line, is_dir_only))

    def is_ignored(self, rel_path: Path) -> bool:
        """
        判断相对路径是否被忽略。
        支持通配符、**、目录匹配、否定规则。
        """
        if not self.has_gitignore:
            return False

        # 统一使用 Posix 风格路径分隔符进行匹配
        path_parts = rel_path.parts
        path_str = rel_path.as_posix()
        path_str_dir = path_str + '/'

        ignored = False

        for is_negation, pattern, is_dir_only in self.rules:
            match = False

            # 处理绝对路径模式（以 / 开头）
            if pattern.startswith('/'):
                # 仅匹配从根目录开始的相对路径
                pat = pattern[1:]
                if is_dir_only:
                    match = fnmatch.fnmatch(path_str_dir, pat + '/*') or path_str == pat
                else:
                    match = fnmatch.fnmatch(path_str, pat) or fnmatch.fnmatch(path_str_dir, pat + '/*')
                    # 也匹配作为目录前缀
                    if not match and path_str.startswith(pat + '/'):
                        match = True
            else:
                # 匹配任意路径层级
                # 先检查完整路径
                if is_dir_only:
                    match = fnmatch.fnmatch(path_str_dir, '*/' + pattern + '/*') or \
                            fnmatch.fnmatch(path_str_dir, pattern + '/*') or \
                            path_str == pattern
                else:
                    # 检查文件名、目录名或完整路径
                    if fnmatch.fnmatch(path_str, pattern) or \
                       fnmatch.fnmatch(path_str, '*/' + pattern) or \
                       fnmatch.fnmatch(path_str_dir, '*/' + pattern + '/*'):
                        match = True
                    # 检查路径中的任意部分
                    for part in path_parts:
                        if fnmatch.fnmatch(part, pattern):
                            match = True
                            break

            if match:
                ignored = not is_negation  # 否定规则取消忽略

        return ignored


# ============ 文件扫描 ============
def collect_cpp_files(repo_root: Path, parser: GitIgnoreParser) -> List[Path]:
    """递归收集未被忽略的 C/C++ 文件，返回相对路径列表。"""
    files = []
    
    for root, dirs, filenames in os.walk(repo_root):
        root_path = Path(root).resolve()
        rel_root = root_path.relative_to(repo_root)

        # 过滤被忽略的目录（避免进入）
        dirs_to_remove = []
        for d in dirs:
            rel_dir = rel_root / d if rel_root != Path('.') else Path(d)
            if parser.is_ignored(rel_dir):
                dirs_to_remove.append(d)
        
        for d in dirs_to_remove:
            dirs.remove(d)

        # 收集文件
        for fname in filenames:
            if Path(fname).suffix.lower() not in CPP_EXTENSIONS:
                continue
            
            rel_file = rel_root / fname if rel_root != Path('.') else Path(fname)
            if parser.is_ignored(rel_file):
                continue

            #ignore encoding_convert.cpp
            if rel_file.name == "encoding_convert.cpp":
                continue
            
            files.append(rel_file)

    return sorted(files)


# ============ 格式化执行 ============
def format_file(repo_root: Path, rel_path: Path, clang_format: str, style: str, dry_run: bool) -> Tuple[bool, str, str]:
    """
    格式化单个文件。
    返回: (success, rel_path, message)
    """
    abs_path = repo_root / rel_path
    cmd = [clang_format, '-i', str(abs_path)]
    if style:
        cmd.extend(['--style', style])

    if dry_run:
        return True, str(rel_path), "[DRY-RUN] 将格式化"

    try:
        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True
        )
        return True, str(rel_path), "已格式化"
    except subprocess.CalledProcessError as e:
        return False, str(rel_path), f"失败: {e.stderr.strip()}"
    except FileNotFoundError:
        return False, str(rel_path), "错误: clang-format 未找到"


def main():
    parser = argparse.ArgumentParser(
        description='批量格式化 C/C++ 文件，自动排除 .gitignore 内容',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s                          # 格式化当前目录，使用 .clang-format 文件
  %(prog)s -p /path/to/project      # 格式化指定项目
  %(prog)s --style LLVM             # 使用 LLVM 风格（而非项目 .clang-format）
  %(prog)s -j 8                     # 使用 8 线程并行格式化
  %(prog)s --dry-run                # 仅列出将被格式化的文件，不执行
        """
    )
    parser.add_argument('-p', '--path', default='.', help='项目根目录（默认当前目录）')
    parser.add_argument('--style', default='', help='指定 clang-format 风格（如 LLVM, Google, Chromium, Mozilla, WebKit）')
    parser.add_argument('-j', '--jobs', type=int, default=4, help='并行线程数（默认 4）')
    parser.add_argument('--clang-format', default='clang-format', help='clang-format 可执行文件路径')
    parser.add_argument('--dry-run', action='store_true', help='仅预览，不实际格式化')
    parser.add_argument('-v', '--verbose', action='store_true', help='显示每个文件的详细结果')

    args = parser.parse_args()

    repo_root = Path(args.path).resolve()
    if not repo_root.exists():
        print(f"错误: 路径不存在 {repo_root}")
        sys.exit(1)

    # 检查 clang-format 可用性
    if not args.dry_run:
        try:
            subprocess.run([args.clang_format, '--version'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
        except (subprocess.CalledProcessError, FileNotFoundError):
            print(f"错误: 无法执行 clang-format: {args.clang_format}")
            print("请确保 LLVM/Clang 已安装并加入 PATH")
            sys.exit(1)

    # 解析 .gitignore
    gitignore = GitIgnoreParser(repo_root)
    if gitignore.has_gitignore:
        print(f"已加载 .gitignore: {repo_root / '.gitignore'}")
    else:
        print("未找到 .gitignore，将处理所有文件")

    # 收集文件
    print("正在扫描 C/C++ 文件...")
    files = collect_cpp_files(repo_root, gitignore)
    
    if not files:
        print("未找到需要格式化的 C/C++ 文件")
        sys.exit(0)

    print(f"发现 {len(files)} 个文件待格式化")
    print("-" * 50)

    # 执行格式化
    success_count = 0
    fail_count = 0

    if args.jobs == 1:
        # 单线程
        for rel in files:
            ok, path, msg = format_file(repo_root, rel, args.clang_format, args.style, args.dry_run)
            if ok:
                success_count += 1
            else:
                fail_count += 1
            if args.verbose or not ok:
                print(f"  [{ '✓' if ok else '✗' }] {path} {msg}")
    else:
        # 多线程
        with ThreadPoolExecutor(max_workers=args.jobs) as executor:
            future_to_file = {
                executor.submit(format_file, repo_root, rel, args.clang_format, args.style, args.dry_run): rel
                for rel in files
            }
            
            for future in as_completed(future_to_file):
                ok, path, msg = future.result()
                if ok:
                    success_count += 1
                else:
                    fail_count += 1
                if args.verbose or not ok:
                    print(f"  [{ '✓' if ok else '✗' }] {path} {msg}")

    print("-" * 50)
    mode = "预览模式" if args.dry_run else "完成"
    print(f"{mode}: 成功 {success_count} 个, 失败 {fail_count} 个, 总计 {len(files)} 个")


if __name__ == '__main__':
    main()