#!/usr/bin/env python3

# +--------------------------------------------------------------------------
#
# File:        audit_iwyu.py
#
# NightDriverStrip - (c) 2026 Plummer's Software LLC.  All Rights Reserved.
#
# This file is part of the NightDriver software project.
#
#    NightDriver is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    NightDriver is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with Nightdriver.  It is normally found in copying.txt
#    If not, see <https://www.gnu.org/licenses/>.
#
# Description:
#
#    Run include-what-you-use against one or more PlatformIO environments.
#
#    The script first regenerates PlatformIO's compilation database for each
#    selected environment, then runs iwyu_tool.py with include-what-you-use on
#    the project's translation units and headers.
#
# ---------------------------------------------------------------------------

import argparse
import json
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import show_envs

PROJECT_ROOT = Path(__file__).resolve().parent.parent
IWYU_POLICY_FILE = PROJECT_ROOT / 'tools' / 'iwyu.policy'
HEADER_SUFFIXES = {'.h', '.hh', '.hpp', '.inc'}
DEFAULT_KEEP_GLOBS = [
    'globals.h',
    'WString.h',
]
DEFAULT_IWYU_ARGS = [
    '--experimental=clang_mappings',
]
SUPPRESSED_OUTPUT_TOKENS = (
    'Experimental flag enabled: \'clang_mappings\'',
    '#include "globals.h"',
    '#include <globals.h>',
    '#include "WString.h"',
    '#include <WString.h>',
    'vector.tcc',
    'stl_uninitialized.h',
    'hashtable_policy',
    'basic_string.tcc',
    'string_view.tcc',
    'alloc_traits.h',
    'ptr_traits.h',
)
COMPAT_DEFINE_FLAGS = [
    '-D__gnuc_va_list=__builtin_va_list',
    '-D__block=__iwyu_block',
]
IWYU_KEEP_PATTERNS = (
    ' should add these lines:',
    ' should remove these lines:',
    ' has correct #includes/fwd-decls',
    'The full include-list for ',
)


def run_command(command_args):
    return subprocess.run(command_args, cwd=PROJECT_ROOT, check=False)


def split_command(command):
    return shlex.split(command)


def discover_headers():
    headers = []
    include_dirs = [PROJECT_ROOT / 'include', PROJECT_ROOT / 'src']

    for base_dir in include_dirs:
        if not base_dir.exists():
            continue

        for path in base_dir.rglob('*'):
            if not path.is_file():
                continue
            if path.suffix not in HEADER_SUFFIXES:
                continue
            if 'src/uzlib' in path.as_posix():
                continue
            headers.append(path.relative_to(PROJECT_ROOT).as_posix())

    return sorted(set(headers))


def load_iwyu_policy(policy_file=IWYU_POLICY_FILE):
    keep_globs = list(DEFAULT_KEEP_GLOBS)
    mapping_files = []

    if not policy_file.is_file():
        return keep_globs, mapping_files

    for raw_line in policy_file.read_text(encoding='utf-8').splitlines():
        line = raw_line.split('#', 1)[0].strip()
        if not line:
            continue

        keyword, *rest = line.split(maxsplit=1)
        value = rest[0].strip() if rest else ''
        if not value:
            continue

        if keyword == 'keep' and value not in keep_globs:
            keep_globs.append(value)
        elif keyword == 'mapping' and value not in mapping_files:
            mapping_files.append(value)

    return keep_globs, mapping_files


def strip_unsupported_flags(command_args):
    filtered_args = []
    skip_next = False

    for index, arg in enumerate(command_args):
        if skip_next:
            skip_next = False
            continue

        if arg == '-include' and index + 1 < len(command_args) and command_args[index + 1] == 'string.h':
            skip_next = True
            continue
        if arg == '-o':
            skip_next = True
            continue
        if arg.startswith(('-f', '-m', '-W', '-O', '-g')):
            continue
        if arg in {'-c', '-o', '-MMD', '-MD'}:
            continue
        filtered_args.append(arg)

    return filtered_args


def discover_toolchain_include_dirs(compiler_path):
    compiler_path = Path(compiler_path)
    toolchain_root = compiler_path.parent.parent

    if not toolchain_root.exists():
        return []

    triple_dirs = [path for path in toolchain_root.iterdir() if path.is_dir() and path.name.endswith('-elf')]
    if not triple_dirs:
        return []

    compiler_name = compiler_path.name
    triple_dir = next((path for path in triple_dirs if path.name in compiler_name), triple_dirs[0])

    include_dirs = [
        triple_dir / 'include',
        triple_dir / 'sys-include',
        toolchain_root / 'include',
    ]

    gcc_include_dirs = sorted(toolchain_root.glob(f'lib/gcc/{triple_dir.name}/*/include'))
    for gcc_include_dir in gcc_include_dirs:
        include_dirs.append(gcc_include_dir)
        fixed_dir = gcc_include_dir.parent / 'include-fixed'
        if fixed_dir.is_dir():
            include_dirs.append(fixed_dir)

    cxx_root = triple_dir / 'include' / 'c++'
    if cxx_root.is_dir():
        for version_dir in sorted(path for path in cxx_root.iterdir() if path.is_dir()):
            include_dirs.append(version_dir)
            for nested_dir in sorted(path for path in version_dir.iterdir() if path.is_dir()):
                include_dirs.append(nested_dir)

    return [
        path.as_posix()
        for path in include_dirs
        if path.is_dir()
    ]


def detect_toolchain_triple(compiler_path):
    compiler_path = Path(compiler_path)
    compiler_name = compiler_path.name

    for suffix in ('-gcc', '-g++', '-c++'):
        if compiler_name.endswith(suffix):
            return compiler_name[:-len(suffix)]

    if compiler_name.endswith('clang') or compiler_name.endswith('clang++'):
        return None

    return None


def detect_driver_mode(source_path, compiler_path):
    source_suffix = Path(source_path).suffix.lower()
    if source_suffix in {'.c', '.s', '.S'} or compiler_path.endswith('gcc'):
        return 'gcc'
    return 'g++'


def rewrite_compile_command(entry):
    command_string = entry.get('command')
    if not command_string and 'arguments' in entry:
        command_string = shlex.join(entry['arguments'])
    if not command_string:
        return command_string

    command_args = split_command(command_string)
    if not command_args:
        return command_string

    compiler_path = Path(command_args[0])
    compiler_args = command_args[1:]

    if compiler_path.name == 'ccache' and len(command_args) > 1:
        compiler_path = Path(command_args[1])
        compiler_args = command_args[2:]

    driver_mode = detect_driver_mode(entry.get('file', ''), compiler_path.as_posix())
    toolchain_root = compiler_path.parent.parent
    toolchain_triple = detect_toolchain_triple(compiler_path)
    compiler_args = strip_unsupported_flags(compiler_args)
    include_dirs = discover_toolchain_include_dirs(compiler_path)

    rewritten_args = [compiler_path.as_posix()]
    if toolchain_triple:
        rewritten_args.extend([f'--driver-mode={driver_mode}', f'--target={toolchain_triple}', f'--gcc-toolchain={toolchain_root.as_posix()}'])
    elif driver_mode:
        rewritten_args.append(f'--driver-mode={driver_mode}')

    rewritten_args.append('-w')
    rewritten_args.extend(COMPAT_DEFINE_FLAGS)

    for include_dir in include_dirs:
        rewritten_args.extend(['-isystem', include_dir])

    rewritten_args.extend(compiler_args)
    return shlex.join(rewritten_args)


def rewrite_compilation_database(compilation_db, output_dir):
    rewritten_db = []

    for entry in compilation_db:
        rewritten_entry = dict(entry)
        rewritten_entry['command'] = rewrite_compile_command(rewritten_entry)
        rewritten_entry.pop('arguments', None)
        rewritten_db.append(rewritten_entry)

    output_path = output_dir / 'compile_commands.json'
    output_path.write_text(json.dumps(rewritten_db, indent=2))
    return output_path


import re

def remove_no_op_includes(text):
    output_blocks = []
    for block in text.split('---\n'):
        lines = block.splitlines()
        add_lines = set()
        remove_lines = set()
        in_add = False
        in_remove = False
        has_minus_line = False

        for line in lines:
            if 'should add these lines:' in line:
                in_add = True
                in_remove = False
            elif 'should remove these lines:' in line:
                in_add = False
                in_remove = True
            elif in_remove and line.startswith('- '):
                has_minus_line = True

            if '#include' in line:
                match = re.search(r'#include\s+([<"][^>"]+[>"])', line)
                if match:
                    inc_file = match.group(1)
                    if in_add:
                        add_lines.add(inc_file)
                    elif in_remove and line.startswith('- '):
                        remove_lines.add(inc_file)

        common = add_lines.intersection(remove_lines)
        if not common and has_minus_line:
            output_blocks.append(block)
            continue
        elif not has_minus_line and remove_lines:
            # The user requested that we ignore blocks where there are no '- ' lines
            # in the remove section, as IWYU is likely confused and suggesting deleting everything.
            # But if there are NO remove lines at all, we should still process the add section.
            continue

        new_lines = []
        for line in lines:
            if '#include' in line:
                match = re.search(r'#include\s+([<"][^>"]+[>"])', line)
                if match and match.group(1) in common:
                    continue
            new_lines.append(line)

        output_blocks.append('\n'.join(new_lines))

    return '---\n'.join(output_blocks)

def filter_iwyu_output(output_text):
    filtered_lines = []
    warning_block = False

    for line in output_text.splitlines():
        if any(token in line for token in SUPPRESSED_OUTPUT_TOKENS):
            continue

        if line.startswith('warning:') or ' warning:' in line:
            warning_block = True
            continue

        if warning_block:
            if not line.strip() or line.startswith('error:') or line.startswith('In file included from'):
                warning_block = False
            else:
                continue

        if line.startswith('note:') or ' note:' in line:
            continue

        if line.startswith('The full include-list for '):
            continue

        if line.startswith('error:') or ' error:' in line:
            filtered_lines.append(line)
            continue

        if any(pattern in line for pattern in IWYU_KEEP_PATTERNS):
            filtered_lines.append(line)
            continue

        if line.startswith(('In file included from', ' ', '\t', '/')):
            if filtered_lines and (filtered_lines[-1].startswith('error:') or filtered_lines[-1].startswith('In file included from')):
                filtered_lines.append(line)
            continue

        if line.strip():
            filtered_lines.append(line)

    raw_filtered = '\n'.join(filtered_lines)
    return remove_no_op_includes(raw_filtered)


def build_iwyu_args(extra_iwyu_args, mapping_files, keep_globs, check_also_headers):
    iwyu_args = []

    for arg in DEFAULT_IWYU_ARGS:
        iwyu_args.extend(['-Xiwyu', arg])

    for arg in extra_iwyu_args:
        iwyu_args.extend(['-Xiwyu', arg])

    for mapping_file in mapping_files:
        iwyu_args.extend(['-Xiwyu', f'--mapping_file={mapping_file}'])

    for keep_glob in DEFAULT_KEEP_GLOBS:
        iwyu_args.extend(['-Xiwyu', f'--keep={keep_glob}'])

    for keep_glob in keep_globs:
        iwyu_args.extend(['-Xiwyu', f'--keep={keep_glob}'])

    for header in check_also_headers:
        iwyu_args.extend(['-Xiwyu', f'--check_also={header}'])

    return iwyu_args


def main():
    parser = argparse.ArgumentParser(
        description='Run include-what-you-use against PlatformIO environments.'
    )
    parser.add_argument(
        '-e',
        '--env',
        dest='envs',
        action='append',
        help='PlatformIO environment to audit. May be passed multiple times.',
    )
    parser.add_argument(
        '-j',
        '--jobs',
        type=int,
        default=0,
        help='Concurrent IWYU jobs inside iwyu_tool.py. Default 0 uses the logical core count; output is captured per env, but job ordering inside one env is not guaranteed.',
    )
    parser.add_argument(
        '--verbose',
        action='store_true',
        help='Print the IWYU command lines as they are launched.',
    )
    parser.add_argument(
        '--no-header-sweep',
        action='store_true',
        help='Only audit translation units from src/ and skip header check_also entries.',
    )
    parser.add_argument(
        '--iwyu-tool',
        default='iwyu_tool.py',
        help='Path to iwyu_tool.py. Default assumes it is on PATH.',
    )
    parser.add_argument(
        '--iwyu-arg',
        dest='iwyu_args',
        action='append',
        default=[],
        help='Additional raw IWYU option passed after -Xiwyu. May be repeated.',
    )
    parser.add_argument(
        '--iwyu-mapping-file',
        dest='mapping_files',
        action='append',
        default=[],
        help='IWYU mapping file passed as --mapping_file. May be repeated.',
    )
    parser.add_argument(
        '--iwyu-keep',
        dest='keep_globs',
        action='append',
        default=[],
        help='Additional IWYU include glob passed as --keep. May be repeated. globals.h and WString.h are kept by default.',
    )
    args = parser.parse_args()

    if shutil.which('pio') is None:
        print('Error: pio was not found on PATH.')
        return 2

    if shutil.which(args.iwyu_tool) is None and not Path(args.iwyu_tool).exists():
        print(f"Error: {args.iwyu_tool} was not found on PATH or as a file path.")
        return 2

    envs = args.envs or show_envs.getenvs()

    if not envs:
        print('Error: no PlatformIO environments were found.')
        return 2

    policy_keep_globs, policy_mapping_files = load_iwyu_policy()
    headers = [] if args.no_header_sweep else discover_headers()
    iwyu_tail = build_iwyu_args(
        args.iwyu_args,
        policy_mapping_files + args.mapping_files,
        policy_keep_globs + args.keep_globs,
        headers,
    )

    findings = []
    errors = []

    for env in envs:
        print()
        print('=' * 79)
        print(f'Environment: {env}')
        print('=' * 79)

        compiledb_result = run_command(['pio', 'run', '-t', 'compiledb', '-e', env])
        compilation_db_path = PROJECT_ROOT / 'compile_commands.json'
        if compiledb_result.returncode != 0:
            if not compilation_db_path.exists():
                errors.append(f"{env}: failed to generate compile_commands.json (exit {compiledb_result.returncode})")
                continue

            print(f'{env}: PlatformIO database generation failed, using existing compile_commands.json.')

        with tempfile.TemporaryDirectory(prefix=f'iwyu-{env}-', dir=PROJECT_ROOT) as temp_dir_name:
            temp_dir = Path(temp_dir_name)

            try:
                compilation_db = json.loads(compilation_db_path.read_text(encoding='utf-8'))
            except (OSError, json.JSONDecodeError) as error:
                errors.append(f'{env}: failed to read compile_commands.json ({error})')
                continue

            rewrite_compilation_database(compilation_db, temp_dir)

            iwyu_command = [
                args.iwyu_tool,
                '-p',
                temp_dir.as_posix(),
                '-j',
                str(args.jobs),
                '-e',
                '.pio',
                '-e',
                'src/uzlib',
                'src',
                '--',
            ]
            iwyu_command.extend(iwyu_tail)

            if args.verbose:
                print('Running:', ' '.join(iwyu_command))

            iwyu_result = subprocess.run(iwyu_command, cwd=PROJECT_ROOT, check=False, capture_output=True, text=True)
            filtered_output = filter_iwyu_output((iwyu_result.stdout or '') + (iwyu_result.stderr or ''))
            if filtered_output.strip():
                print(filtered_output)

            if iwyu_result.returncode == 0:
                print(f'{env}: no IWYU violations reported.')
            elif iwyu_result.returncode == 1:
                findings.append(env)
            else:
                errors.append(f'{env}: IWYU failed with exit code {iwyu_result.returncode}')

    print()
    print('=' * 79)
    print('Summary')
    print('=' * 79)

    if findings:
        print('IWYU reported suggestions for:')
        for env in findings:
            print(f'* {env}')

    if errors:
        print('Errors:')
        for error in errors:
            print(f'* {error}')

    if errors:
        return 2
    if findings:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
