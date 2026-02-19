#!/usr/bin/env python3
"""Create a new TODO item in TODO/ITEMS/ with a serialized number."""

import argparse
import os
import re
import sys
from datetime import date

TODO_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "TODO")
ITEMS_DIR = os.path.join(TODO_DIR, "ITEMS")
DONE_DIR = os.path.join(TODO_DIR, "DONE")
IN_PROGRESS_DIR = os.path.join(TODO_DIR, "IN_PROGRESS")


def get_next_id() -> int:
    """Scan all status folders and return the next available item number."""
    search_dirs = [ITEMS_DIR, DONE_DIR, IN_PROGRESS_DIR]
    max_id = 0
    pattern = re.compile(r"^(\d{4})_")
    for d in search_dirs:
        if not os.path.isdir(d):
            continue
        for name in os.listdir(d):
            m = pattern.match(name)
            if m:
                max_id = max(max_id, int(m.group(1)))
    return max_id + 1


def slugify(name: str) -> str:
    """Convert a name to a lowercase hyphenated slug."""
    slug = name.lower().strip()
    slug = re.sub(r"[^a-z0-9]+", "-", slug)
    slug = slug.strip("-")
    return slug


def create_item(name: str) -> None:
    item_id = get_next_id()
    id_str = f"{item_id:04d}"
    slug = slugify(name)
    filename = f"{id_str}_{slug}.md"
    filepath = os.path.join(ITEMS_DIR, filename)

    os.makedirs(ITEMS_DIR, exist_ok=True)

    today = date.today().isoformat()
    content = f"""---
id: "{id_str}"
title: {name}
created: {today}
---

# [{id_str}] {name}

## Description

What needs to be done and why.

## Links

"""

    with open(filepath, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"Created: {filepath}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Create a new TODO item.")
    parser.add_argument("name", nargs="?", help="Name of the new TODO item")
    args = parser.parse_args()

    name = args.name
    if not name:
        try:
            name = input("Item name: ").strip()
        except (KeyboardInterrupt, EOFError):
            print()
            sys.exit(0)

    if not name:
        print("Error: item name cannot be empty.", file=sys.stderr)
        sys.exit(1)

    create_item(name)


if __name__ == "__main__":
    main()
