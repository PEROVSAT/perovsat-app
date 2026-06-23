'''west dbuild — per-device mode build wrapper for PerovSat.'''

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
from pathlib import Path

import yaml
from west.commands import WestCommand
from west.manifest import Manifest

APP_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONFIG = APP_ROOT / 'dbuild.yml'
SNIPPETS_ROOT = APP_ROOT / 'snippets'
DEFAULT_CMAKE_ARGS = ['-DCMAKE_EXPORT_COMPILE_COMMANDS=ON']


# ── Entry point ───────────────────────────────────────────────────────────────

class Dbuild(WestCommand):
    def __init__(self):
        super().__init__(
            'dbuild',
            'build with per-device modes from dbuild.yml',
            description='Resolve per-device modes from dbuild.yml and run west build.',
            accepts_unknown_args=True,
        )

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name,
            formatter_class=argparse.RawDescriptionHelpFormatter,
            description=self.description,
            epilog=(
                'dbuild is a thin wrapper around west build: it resolves device '
                'snippets and backend Kconfig symbols, then runs west build with '
                'the same flags you would use directly. Any option not listed above '
                'is forwarded to west build (for example: -t run, -c, -o=-j8). Use '
                '-- only when passing additional raw cmake arguments, as with west '
                'build itself.\n\n'
                'Examples:\n'
                '  west dbuild -b nucleo_u575zi_q\n'
                '  west dbuild -b nucleo_u575zi_q -t run\n'
                '  west dbuild -b nucleo_u575zi_q -- -DCONFIG_LOG_DEFAULT_LEVEL=4'
            ),
        )

        parser.add_argument(
            '-b', '--board', required=True,
            help='board to build for (e.g. nucleo_u575zi_q)',
        )
        parser.add_argument(
            '--config',
            default=str(DEFAULT_CONFIG),
            help='path to dbuild.yml (default: dbuild.yml)',
        )
        parser.add_argument(
            '-d', '--build-dir',
            help='build directory to create or use',
        )
        parser.add_argument(
            '-p', '--pristine',
            choices=['auto', 'always', 'never'],
            default='always',
            help='pristine build directory before building (default: always)',
        )
        parser.add_argument(
            '--dry-run',
            action='store_true',
            help='print the resolved west build command without running it',
        )

        return parser

    def do_run(self, args, unknown):
        # 1. Load and validate dbuild.yml
        try:
            selections, device_map = load_dbuild_config(Path(args.config))
            validate_device_map(device_map)
        except (FileNotFoundError, ValueError) as exc:
            self.die(str(exc))

        # 2. Resolve selections to snippets and backend Kconfig CMake args
        try:
            snippets, cmake_kconfig_args = resolve_build_config(
                selections, device_map, args.board,
            )
        except (FileNotFoundError, ValueError) as exc:
            self.die(str(exc))

        self.inf('Resolved snippets:', ' '.join(snippets))
        self.inf('Resolved backend Kconfig:', ' '.join(cmake_kconfig_args))

        # 3. Construct and run west build
        cmd = build_west_command(
            args.board,
            snippets,
            cmake_kconfig_args,
            args.build_dir,
            args.pristine,
            unknown,
        )

        if args.dry_run:
            self.inf('Command:', shlex.join(cmd))
            return

        self.inf('Running:', shlex.join(cmd))
        try:
            subprocess.run(cmd, check=True)
        except subprocess.CalledProcessError as exc:
            sys.exit(exc.returncode)


# ── Configuration ─────────────────────────────────────────────────────────────

def load_dbuild_config(path: Path) -> tuple[dict[str, str], dict]:
    '''Load dbuild.yml and return (selections, device_map).'''
    if not path.is_file():
        raise FileNotFoundError(f'dbuild config not found: {path}')

    with path.open() as f:
        data = yaml.safe_load(f)

    if not isinstance(data, dict):
        raise ValueError(f'{path}: expected a YAML mapping at the top level')
    if 'selections' not in data:
        raise ValueError(f'{path}: missing required top-level "selections" key')
    if 'devices' not in data:
        raise ValueError(f'{path}: missing required top-level "devices" key')

    selections = data['selections']
    if not isinstance(selections, dict) or not selections:
        raise ValueError(f'{path}: "selections" must be a non-empty mapping')

    return {k: str(v).lower() for k, v in selections.items()}, data['devices']


def validate_device_map(device_map: dict) -> None:
    '''Ensure every device entry and its modes define required fields.'''
    errors: list[str] = []

    for device, entry in device_map.items():
        if not isinstance(entry, dict):
            errors.append(f'device {device!r}: expected a mapping')
            continue

        if 'west_project' not in entry:
            errors.append(f'device {device!r}: missing required "west_project"')

        modes = entry.get('modes')
        if not isinstance(modes, dict) or not modes:
            errors.append(f'device {device!r}: missing or empty "modes" mapping')
            continue

        for mode, mode_entry in modes.items():
            if not isinstance(mode_entry, dict):
                errors.append(f'device {device!r} mode {mode!r}: invalid entry')
                continue
            for field in ('snippet', 'kconfig_backend'):
                if field not in mode_entry:
                    errors.append(
                        f'device {device!r} mode {mode!r}: missing required "{field}"'
                    )

    if errors:
        raise ValueError('\n'.join(errors))


# ── Build resolution ──────────────────────────────────────────────────────────

def resolve_build_config(
    selections: dict[str, str],
    device_map: dict,
    board: str,
    west_projects: dict | None = None,
) -> tuple[list[str], list[str]]:
    '''Resolve device selections to snippets and backend Kconfig CMake args.'''
    board_name = board_short_name(board)
    snippets: list[str] = []
    cmake_args: list[str] = []
    errors: list[str] = []

    if west_projects is None:
        west_projects = load_west_project_index()

    for device, mode in selections.items():
        if device not in device_map:
            errors.append(
                f'unknown device {device!r} in selections '
                f'(not defined under "devices" in dbuild.yml)'
            )
            continue

        device_entry = device_map[device]
        modes = device_entry.get('modes', {})
        if mode not in modes:
            valid = ', '.join(sorted(modes))
            errors.append(
                f'invalid mode {mode!r} for device {device!r} '
                f'(valid modes: {valid})'
            )
            continue

        mode_entry = modes[mode]
        snippet_name = mode_entry['snippet']
        snippet_path = snippet_dir(snippet_name)

        if not snippet_path.is_dir():
            errors.append(
                f'device {device!r} mode {mode!r} requires snippet '
                f'{snippet_name!r}, but {snippet_path} does not exist'
            )
            continue

        west_project = device_entry['west_project']
        west_error = validate_west_project(device, mode, west_project, west_projects)
        if west_error:
            errors.append(west_error)
            continue

        if mode_entry.get('board_overlay_required', False):
            if not snippet_has_board_overlay(snippet_path, board_name):
                supported = sorted(
                    p.stem for p in (snippet_path / 'boards').glob('*.overlay')
                ) if (snippet_path / 'boards').is_dir() else []

                errors.append(
                    f'device {device!r} mode {mode!r} (snippet {snippet_name!r}) '
                    f'is not supported on board {board_name!r}; '
                    f'supported boards: {", ".join(supported) if supported else "(none)"}'
                )
                continue

        cmake_args.append(f'-D{mode_entry["kconfig_backend"]}=y')
        snippets.append(snippet_name)

    if errors:
        raise ValueError('\n'.join(errors))

    return snippets, cmake_args


def validate_west_project(
    device: str,
    mode: str,
    west_project: str,
    west_projects: dict,
) -> str | None:
    '''Return an error message if a required west project is unavailable.'''
    if west_project not in west_projects:
        return (
            f'dbuild.yml references west project {west_project!r} for '
            f'{device!r}, but it is not listed in west.yml'
        )

    project = west_projects[west_project]
    if not project.is_cloned():
        return (
            f'device {device!r} mode {mode!r} requires west project '
            f'{west_project!r}, which is not cloned in this workspace. '
            f'If you lack access, choose a different mode. '
            f'If you should have access, run: west update'
        )

    module_yml = Path(project.abspath) / 'zephyr' / 'module.yml'
    if not module_yml.is_file():
        return (
            f'device {device!r} mode {mode!r} requires west project '
            f'{west_project!r}, but {module_yml} is missing'
        )

    return None


def load_west_project_index() -> dict:
    '''Return a name -> Project map for all west manifest projects.'''
    manifest = Manifest.from_file()
    return {project.name: project for project in manifest.get_projects([])}


def snippet_has_board_overlay(snippet_path: Path, board: str) -> bool:
    '''Return True if the snippet declares board overlay coverage for this board.'''
    if not (snippet_path / 'boards' / f'{board}.overlay').is_file():
        return False

    snippet_yml = snippet_path / 'snippet.yml'
    if not snippet_yml.is_file():
        return False

    with snippet_yml.open() as f:
        data = yaml.safe_load(f) or {}

    return any(key.split('/')[0] == board for key in data.get('boards', {}))


# ── Command construction ──────────────────────────────────────────────────────

def build_west_command(
    board: str,
    snippets: list[str],
    cmake_kconfig_args: list[str],
    build_dir: str | None,
    pristine: str | None,
    extra_args: list[str],
) -> list[str]:
    '''Construct the west build command line.'''
    west_extra, cmake_extra = split_west_and_cmake_extra(extra_args)

    cmd = ['west', 'build', '-b', board]

    for snippet in snippets:
        cmd.extend(['-S', snippet])

    if build_dir:
        cmd.extend(['-d', build_dir])

    if pristine:
        cmd.extend(['-p', pristine])

    cmd.extend(west_extra)
    cmd.append(str(APP_ROOT))

    cmd.append('--')
    cmd.extend(DEFAULT_CMAKE_ARGS + cmake_kconfig_args + cmake_extra)

    return cmd


def split_west_and_cmake_extra(extra_args: list[str]) -> tuple[list[str], list[str]]:
    '''Split passthrough args into west build options and cmake options.

    Only arguments after an explicit ``--`` are forwarded to cmake.
    '''
    if '--' in extra_args:
        sep = extra_args.index('--')
        return extra_args[:sep], extra_args[sep + 1:]
    return extra_args, []


# ── Utilities ─────────────────────────────────────────────────────────────────

def board_short_name(board: str) -> str:
    '''Return the board short name, stripping revision and qualifiers.'''
    return board.split('@')[0].split('/')[0]


def snippet_dir(snippet_name: str) -> Path:
    return SNIPPETS_ROOT / snippet_name
