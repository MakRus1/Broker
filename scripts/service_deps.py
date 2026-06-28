#!/usr/bin/env python3
"""Обновляет .github/service-deps.yaml, docker-compose.yml и CMakeLists.txt."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEPS_FILE = ROOT / ".github" / "service-deps.yaml"
COMPOSE_FILE = ROOT / "docker-compose.yml"
CMAKE_FILE = ROOT / "CMakeLists.txt"
POSTGRES_BASE_PORT = 15433


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write_text(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")


def list_yaml_items(block: str, indent: str = "      - ") -> list[str]:
    items = []
    for line in block.splitlines():
        if line.startswith(indent):
            items.append(line[len(indent):].strip())
    return items


def render_yaml_items(items: list[str], indent: str = "      - ") -> str:
    return "\n".join(f"{indent}{item}" for item in sorted(set(items)))


def parse_libs_common_services(content: str) -> list[str]:
    match = re.search(
        r"^libs:\n  common:\n    services:\n((?:      - .+\n)*)",
        content,
        re.MULTILINE,
    )
    if not match:
        raise SystemExit("Не найдена секция libs.common.services в service-deps.yaml")
    return list_yaml_items(match.group(1))


def replace_libs_common_services(content: str, services: list[str]) -> str:
    block = render_yaml_items(services) + "\n"
    return re.sub(
        r"^libs:\n  common:\n    services:\n(?:      - .+\n)*",
        f"libs:\n  common:\n    services:\n{block}",
        content,
        count=1,
        flags=re.MULTILINE,
    )


def load_deps() -> tuple[str, list[str]]:
    content = read_text(DEPS_FILE)
    libs = parse_libs_common_services(content)
    return content, libs


def save_deps(content: str, libs: list[str]) -> None:
    write_text(DEPS_FILE, replace_libs_common_services(content, sorted(set(libs))))


def service_exists(name: str) -> bool:
    return (ROOT / "services" / name).is_dir()


def next_postgres_port() -> int:
    text = read_text(COMPOSE_FILE)
    ports = [int(p) for p in re.findall(r'"(\d{5}):5432"', text)]
    return max([POSTGRES_BASE_PORT - 1, *ports]) + 1


def add_postgres_compose(service: str) -> None:
    port = next_postgres_port()
    db_name = f"{service}_db_1"
    block = f"""
  postgres-{service}:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: testsuite
      POSTGRES_PASSWORD: testsuite
      POSTGRES_DB: {db_name}
    ports:
      - "{port}:5432"
    volumes:
      - ./services/{service}/postgresql/schemas/db_1.sql:/docker-entrypoint-initdb.d/01_schema.sql:ro
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U testsuite -d {db_name}"]
      interval: 2s
      timeout: 5s
      retries: 10
"""
    content = read_text(COMPOSE_FILE)
    if f"postgres-{service}:" in content:
        return
    content = content.rstrip() + block + "\n"
    header = f"# {service}  -> localhost:{port}\n"
    content = re.sub(
        r"(# orders  -> localhost:15434.*\n)?services:\n",
        lambda m: (m.group(0) if m.group(1) else "services:\n"),
        content,
        count=1,
    )
    if f"# {service}  ->" not in content:
        content = content.replace("services:\n", header + "services:\n", 1)
    write_text(COMPOSE_FILE, content)
    print(f"docker-compose: postgres-{service} на порту {port}")


def add_cmake_subdirectory(service: str) -> None:
    line = f"add_subdirectory(services/{service})"
    content = read_text(CMAKE_FILE)
    if line in content:
        return
    write_text(CMAKE_FILE, content.rstrip() + "\n" + line + "\n")
    print(f"CMakeLists.txt: {line}")


def cmd_add_service(service: str, with_postgres: bool) -> None:
    if not re.fullmatch(r"[a-z][a-z0-9_-]*", service):
        raise SystemExit(f"Некорректное имя сервиса: {service}")
    if not service_exists(service):
        raise SystemExit(f"Сервис не найден: services/{service}")

    content, libs = load_deps()
    if service not in libs:
        libs.append(service)
        save_deps(content, libs)
    add_cmake_subdirectory(service)
    if with_postgres:
        add_postgres_compose(service)
    print(f"service-deps: добавлен {service} в libs.common.services")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    add_svc = sub.add_parser("add-service", help="Зарегистрировать новый сервис")
    add_svc.add_argument("name")
    add_svc.add_argument("--postgresql", action="store_true")

    args = parser.parse_args()
    if args.command == "add-service":
        cmd_add_service(args.name, args.postgresql)


if __name__ == "__main__":
    main()
