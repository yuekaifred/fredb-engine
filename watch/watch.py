#!/usr/bin/env python3
import os
import sys
import time
import re

BAR_WIDTH = 40
REFRESH = 0.2


def human_size(n):
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if n < 1024:
            return f"{n:.1f} {unit}"
        n /= 1024
    return f"{n:.1f} PB"


def read_manifest(directory):
    id_to_level = {}
    try:
        with open(os.path.join(directory, "MANIFEST")) as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) == 2:
                    id_to_level[int(parts[0])] = int(parts[1])
    except FileNotFoundError:
        pass
    return id_to_level


def render(directory):
    try:
        entries = list(os.scandir(directory))
    except FileNotFoundError:
        return f"dir not found or not created yet: {os.path.abspath(directory)}"

    id_to_level = read_manifest(directory)

    files = sorted(
        (e for e in entries if re.fullmatch(r"\d+\.bin", e.name)),
        key=lambda e: (id_to_level.get(int(e.name[:-4]), 0), int(e.name[:-4])),
    )

    if not files:
        return f"segment: {os.path.abspath(directory)}   [0 files]"

    sizes = []
    files_stable = []
    for e in files:
        try:
            sizes.append(e.stat().st_size)
            files_stable.append(e)
        except FileNotFoundError:
            pass
    files = files_stable
    total = sum(sizes)
    largest = max(sizes)

    lines = []
    lines.append(
        f"segment: {os.path.abspath(directory)}   [{len(files)} files, {human_size(total)} total]"
    )

    max_name = max((len(e.name) for e in files), default=5)

    cur_level = None
    for entry, size in zip(files, sizes):
        file_id = int(entry.name[:-4])
        level = id_to_level.get(file_id, 0)
        if level != cur_level:
            lines.append(f"\n  L{level}")
            cur_level = level
        filled = round(size / largest * BAR_WIDTH)
        bar = "█" * filled
        lines.append(f"    {entry.name:<{max_name}}  {bar}  {human_size(size):>9}")

    return "\n".join(lines)


def main():
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} <dir>")
        sys.exit(1)

    directory = sys.argv[1]

    try:
        while True:
            output = render(directory)
            sys.stdout.write("\033[H\033[J")
            sys.stdout.write(output + "\n")
            sys.stdout.flush()
            time.sleep(REFRESH)
    except KeyboardInterrupt:
        print()


if __name__ == "__main__":
    main()
