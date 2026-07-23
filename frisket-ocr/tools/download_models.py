"""Prefetch EasyOCR English models for offline MSI bundling."""

from __future__ import annotations

import argparse

from engine import get_reader, model_storage_directory


def main() -> int:
    parser = argparse.ArgumentParser(description="Download EasyOCR models")
    parser.add_argument("--languages", nargs="+", default=["en"])
    args = parser.parse_args()
    print(f"Downloading models to {model_storage_directory()} ...")
    get_reader(args.languages)
    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
