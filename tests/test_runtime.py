"""Проверка числового парсинга скомпилированным C++20, без моков алгоритма."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'scripts/openapi_gen'))
from render import COMMON


class RuntimeTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which('clang++') or shutil.which('g++'), 'Нужен C++ компилятор')
    def test_scalar_parser_compiles_and_rejects_invalid_numbers(self):
        # Берется генерируемый числовой парсер. Date требует userver и проверяется
        # настоящим contract_service и его скомпилированным клиентом.
        code = COMMON.split('template <typename T> struct IsVector')[0]
        code = '\n'.join(line for line in code.splitlines() if '<userver/' not in line and '<platform/' not in line and 'userver::utils::datetime::Date' not in line and 'std::chrono::system_clock::time_point' not in line and line != '#pragma once')
        code += '''
}
#include <cassert>
int main() {
    using broker_codegen::ParseScalar;
    assert(ParseScalar<std::int64_t>("-42") == -42);
    assert(ParseScalar<double>("1.25") == 1.25);
    assert(!ParseScalar<bool>("false"));
    assert(ParseScalar<std::string>("").empty());
    for (auto value : {"", "1tail", " 1", "1 ", "9223372036854775808"}) {
        bool rejected = false;
        try { ParseScalar<std::int64_t>(value); } catch (const std::exception&) { rejected = true; }
        assert(rejected);
    }
    for (auto value : {"nan", "inf", "1tail", "+1"}) {
        bool rejected = false;
        try { ParseScalar<double>(value); } catch (const std::exception&) { rejected = true; }
        assert(rejected);
    }
    bool rejected = false;
    try { ParseScalar<bool>("False"); } catch (const std::exception&) { rejected = true; }
    assert(rejected);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / 'runtime.cpp';source.write_text(code)
            binary = Path(tmp) / 'runtime'
            compiler = shutil.which('clang++') or shutil.which('g++')
            subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror', str(source), '-o', str(binary)], check=True, capture_output=True)
            subprocess.run([str(binary)], check=True)
