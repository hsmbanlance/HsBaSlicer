#!/usr/bin/env python3
"""
Convert .qoder/repowiki content to GitHub Wiki format.

This script:
1. Reads repowiki markdown files (en/zh)
2. Flattens nested directory structure into wiki-friendly page names
3. Generates Home.md with navigation and _Sidebar.md
4. Outputs to a target directory ready for git push to wiki repo

Usage:
    python repowiki_to_github_wiki.py [--lang en|zh|both] [--src SRC_DIR] [--out OUT_DIR]
"""

import argparse
import json
import os
import re
import shutil
import sys
from pathlib import Path


def sanitize_wiki_name(name: str) -> str:
    """Convert a file/folder name to a GitHub Wiki compatible page name."""
    # Replace spaces and special chars with hyphens
    name = re.sub(r'[<>:"/\\|?*()&]', '', name)
    name = name.replace(' ', '-')
    # Remove trailing dots/underscores
    name = name.strip('.')
    return name


def collect_markdown_files(content_dir: Path) -> list[Path]:
    """Recursively collect all .md files from content directory."""
    return sorted(content_dir.rglob("*.md"))


def relative_page_name(md_file: Path, content_dir: Path) -> str:
    """
    Convert a markdown file path to a wiki page name.
    e.g. 'Core Architecture/FDM Pipeline System.md' -> 'Core-Architecture-FDM-Pipeline-System'
    """
    rel = md_file.relative_to(content_dir)
    parts = list(rel.parts)
    # Remove .md extension from last part
    parts[-1] = parts[-1].removesuffix('.md')
    # Join with hyphens and sanitize each part
    sanitized = [sanitize_wiki_name(p) for p in parts]
    return '-'.join(sanitized)


def clean_content(text: str) -> str:
    """Remove repowiki-specific tags (cite, section sources, diagram sources) for cleaner wiki output."""
    # Remove <cite>...</cite> blocks
    text = re.sub(r'<cite>.*?</cite>', '', text, flags=re.DOTALL)
    # Remove **Section sources** blocks with their bullet lists
    text = re.sub(
        r'\*\*Section sources\*\*\s*\n(?:- \[.*?\]\(file://.*?\)\s*\n?)+',
        '', text
    )
    # Remove **Diagram sources** blocks with their bullet lists
    text = re.sub(
        r'\*\*Diagram sources\*\*\s*\n(?:- \[.*?\]\(file://.*?\)\s*\n?)+',
        '', text
    )
    # Convert file:// links to plain text (they don't work in wiki)
    text = re.sub(r'\[([^\]]+)\]\(file://[^)]+\)', r'`\1`', text)
    # Clean up excessive blank lines
    text = re.sub(r'\n{3,}', '\n\n', text)
    return text.strip()


def build_sidebar(pages: list[tuple[str, str]], lang: str) -> str:
    """
    Build _Sidebar.md content.
    pages: list of (page_name, display_title) tuples
    """
    lang_label = "English" if lang == "en" else "中文"
    lines = [
        f"# HsBaSlicer Wiki ({lang_label})",
        "",
        "**[Home](Home)**",
        "",
    ]

    # Step 1: Identify category index pages and their prefixes.
    # A category index is a page where the folder name equals the file name
    # e.g. "2D-Geometry-Processing-2D-Geometry-Processing" where the prefix
    # "2D-Geometry-Processing-" is the folder part.
    # We also detect categories by checking if a page name is a prefix of others.
    category_indices: dict[str, str] = {}  # prefix -> index_page_name
    page_names = {p for p, _ in pages}

    for page_name, title in pages:
        # Check if this page's name contains a repeated folder-file pattern
        # e.g. "X-X" where X is both folder and file
        parts = page_name.split('-')
        # Try splitting at various points to find folder-file duplication
        for i in range(1, len(parts)):
            prefix = '-'.join(parts[:i])
            suffix = '-'.join(parts[i:])
            if prefix == suffix:
                # This is a category index: prefix is the folder prefix
                category_indices[prefix] = page_name
                break

    # Also detect simple prefix-based categories (for non-duplicated names)
    for page_name, _ in pages:
        if page_name in category_indices.values():
            continue
        for other_name, _ in pages:
            if other_name != page_name and other_name.startswith(page_name + '-'):
                category_indices[page_name] = page_name
                break

    # Step 2: Build category tree
    categories: dict[str, list[tuple[str, str]]] = {
        prefix: [] for prefix in category_indices
    }
    standalone: list[tuple[str, str]] = []
    index_pages = set(category_indices.values())

    for page_name, title in pages:
        if page_name in index_pages:
            continue  # skip category index pages themselves
        # Find the best (longest prefix) matching category
        best_prefix = None
        for prefix in category_indices:
            if page_name.startswith(prefix + '-'):
                if best_prefix is None or len(prefix) > len(best_prefix):
                    best_prefix = prefix
        if best_prefix:
            categories[best_prefix].append((page_name, title))
        else:
            standalone.append((page_name, title))

    # Step 3: Render sidebar
    # Standalone top-level pages
    for page_name, title in standalone:
        lines.append(f"- [{title}]({page_name})")
    if standalone:
        lines.append("")

    # Category sections
    # Use category index page's title as section header
    cat_titles = {prefix: next((t for n, t in pages if n == idx_name), prefix)
                  for prefix, idx_name in category_indices.items()}
    for prefix in sorted(categories.keys()):
        cat_title = cat_titles.get(prefix, prefix)
        idx_page = category_indices[prefix]
        lines.append(f"### [{cat_title}]({idx_page})")
        # Sort children: sub-categories first, then leaf pages
        for sub_name, sub_title in categories[prefix]:
            # Determine display name: strip category prefix for readability
            display = sub_title
            lines.append(f"- [{display}]({sub_name})")
        lines.append("")

    lines.append("---")
    # Language switch link
    if lang == "en":
        lines.append("[中文 Wiki](Home-zh)")
    else:
        lines.append("[English Wiki](Home)")

    return '\n'.join(lines) + '\n'


def build_home_page(pages: list[tuple[str, str]], lang: str) -> str:
    """Build Home.md content."""
    if lang == "en":
        lines = [
            "# HsBaSlicer Wiki",
            "",
            "Welcome to the HsBaSlicer project wiki. "
            "HsBaSlicer is a C++20-based slicing engine for 3D printing and additive manufacturing.",
            "",
            "## Documentation",
            "",
        ]
    else:
        lines = [
            "# HsBaSlicer Wiki",
            "",
            "欢迎使用 HsBaSlicer 项目 Wiki。"
            "HsBaSlicer 是一个基于 C++20 的切片引擎，面向 3D 打印和增材制造领域。",
            "",
            "## 文档目录",
            "",
        ]

    for page_name, title in pages:
        lines.append(f"- [{title}]({page_name})")

    lines.append("")
    if lang == "en":
        lines.append("---")
        lines.append("[中文 Wiki](Home-zh)")
    else:
        lines.append("---")
        lines.append("[English Wiki](Home)")

    return '\n'.join(lines) + '\n'


def extract_title(md_file: Path) -> str:
    """Extract the first H1 title from a markdown file."""
    try:
        with open(md_file, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if line.startswith('# '):
                    return line[2:].strip()
    except Exception:
        pass
    # Fallback: use filename
    return md_file.stem


def process_language(src_dir: Path, content_dir: Path, out_dir: Path, lang: str):
    """Process one language's repowiki content into wiki format."""
    if not content_dir.exists():
        print(f"[WARN] Content directory not found: {content_dir}, skipping.")
        return

    md_files = collect_markdown_files(content_dir)
    if not md_files:
        print(f"[WARN] No markdown files found in {content_dir}, skipping.")
        return

    print(f"[INFO] Processing {lang}: found {len(md_files)} markdown files")

    pages: list[tuple[str, str]] = []  # (wiki_page_name, title)

    for md_file in md_files:
        page_name = relative_page_name(md_file, content_dir)
        title = extract_title(md_file)
        pages.append((page_name, title))

        # Read and clean content
        with open(md_file, 'r', encoding='utf-8') as f:
            content = f.read()
        content = clean_content(content)

        # Write wiki page
        out_file = out_dir / f"{page_name}.md"
        out_file.parent.mkdir(parents=True, exist_ok=True)
        with open(out_file, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"  -> {out_file.name}")

    # Sort pages for consistent ordering
    pages.sort(key=lambda x: x[0])

    # Generate Home page
    home_content = build_home_page(pages, lang)
    if lang == "zh":
        home_file = out_dir / "Home-zh.md"
    else:
        home_file = out_dir / "Home.md"
    with open(home_file, 'w', encoding='utf-8') as f:
        f.write(home_content)
    print(f"  -> {home_file.name}")

    # Generate Sidebar
    sidebar_content = build_sidebar(pages, lang)
    if lang == "zh":
        sidebar_file = out_dir / "_Sidebar-zh.md"
    else:
        sidebar_file = out_dir / "_Sidebar.md"
    with open(sidebar_file, 'w', encoding='utf-8') as f:
        f.write(sidebar_content)
    print(f"  -> {sidebar_file.name}")

    # Generate Footer
    footer_content = (
        "---\n"
        f"*Auto-generated from `.qoder/repowiki/{lang}` by repowiki_to_github_wiki.py*\n"
    )
    footer_file = out_dir / "_Footer.md"
    with open(footer_file, 'w', encoding='utf-8') as f:
        f.write(footer_content)
    print(f"  -> {footer_file.name}")


def main():
    parser = argparse.ArgumentParser(description="Convert repowiki to GitHub Wiki format")
    parser.add_argument(
        '--lang', choices=['en', 'zh', 'both'], default='both',
        help='Language to process (default: both)'
    )
    parser.add_argument(
        '--src', type=Path, default=Path('.qoder/repowiki'),
        help='Source repowiki directory (default: .qoder/repowiki)'
    )
    parser.add_argument(
        '--out', type=Path, default=Path('wiki-output'),
        help='Output directory for wiki files (default: wiki-output)'
    )
    args = parser.parse_args()

    src_dir = args.src.resolve()
    out_dir = args.out.resolve()

    if not src_dir.exists():
        print(f"[ERROR] Source directory not found: {src_dir}")
        sys.exit(1)

    # Clean output directory
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True)

    langs = ['en', 'zh'] if args.lang == 'both' else [args.lang]

    for lang in langs:
        content_dir = src_dir / lang / 'content'
        process_language(src_dir, content_dir, out_dir, lang)

    print(f"\n[DONE] Wiki files generated in: {out_dir}")
    print("Next step: push to GitHub Wiki repo using the publish-wiki workflow.")


if __name__ == '__main__':
    main()
