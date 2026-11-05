#!/usr/bin/env python3
"""Подготовить широкий JSON для браузерного стресс-теста RegistryTable."""

import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rows", type=int, default=100000)
    parser.add_argument("--columns", type=int, default=100)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.rows <= 1000000 or not 1 <= args.columns <= 200:
        parser.error("rows: 1–1000000, columns: 1–200")
    with args.output.open("w") as stream:
        stream.write('{"items":[')
        for index in range(args.rows):
            if index:
                stream.write(",")
            json.dump(
                {
                    "id": str(index),
                    "value": f"Row {index + 1}",
                    "cells": [
                        f"Value {index + 1} field {column + 1}"
                        for column in range(args.columns)
                    ],
                },
                stream,
                separators=(",", ":"),
            )
        stream.write("]}")
    print(f"{args.output}: {args.output.stat().st_size} bytes")


if __name__ == "__main__":
    main()
