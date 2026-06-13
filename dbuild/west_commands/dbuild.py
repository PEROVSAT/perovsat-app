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
DEFAULT_DEVICES_FILE = APP_ROOT / 'dbuild_devices.conf'
DEFAULT_DEVICE_MAP = APP_ROOT / 'dbuild' / 'device_map.yml'
SNIPPETS_ROOT = APP_ROOT / 'snippets'


def board_short_name(board: str) -> str:
    '''Return the board short name, stripping revision and qualifiers.'''
    return board.split('@')[0].split('/')[0]


def parse_devices_conf(path: Path) -> dict[str, str]:
    '''Parse KEY=VALUE lines from dbuild_devices.conf.'''
    devices: dict[str, str] = {}

    if not path.is_file():
        raise FileNotFoundError(f'devices file not found: {path}')

    for line_no, raw in enumerate(path.read_text().splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        if '=' not in line:
            raise ValueError(f'{path}:{line_no}: expected KEY=VALUE, got: {raw!r}')

        key, value = line.split('=', 1)
        key = key.strip()
        value = value.strip().lower()

        if not key:
            raise ValueError(f'{path}:{line_no}: empty device name')
        if key in devices:
            raise ValueError(f'{path}:{line_no}: duplicate device {key!r}')

        devices[key] = value

    if not devices:
        raise ValueError(f'{path}: no device entries found')

    return devices


def load_device_map(path: Path) -> dict:
    '''Load device_map.yml.'''
    if not path.is_file():
        raise FileNotFoundError(f'device map not found: {path}')

    with path.open() as f:
        data = yaml.safe_load(f)

    if not isinstance(data, dict) or 'devices' not in data:
        raise ValueError(f'{path}: missing top-level "devices" key')

    return data['devices']


def validate_device_map(device_map: dict) -> None:
    '''Ensure every mode entry defines required fields.'''
    errors: list[str] = []

    for device, modes in device_map.items():
        if not isinstance(modes, dict):
            errors.append(f'device {device!r}: expected a mode map')
            continue

        for mode, entry in modes.items():
            if not isinstance(entry, dict):
                errors.append(f'device {device!r} mode {mode!r}: invalid entry')
                continue
            if 'snippet' not in entry:
                errors.append(
                    f'device {device!r} mode {mode!r}: missing required snippet'
                )
            if 'west_project' not in entry:
                errors.append(
                    f'device {device!r} mode {mode!r}: missing required west_project'
                )

    if errors:
        raise ValueError('\n'.join(errors))


def snippet_dir(snippet_name: str) -> Path:
    return SNIPPETS_ROOT / snippet_name


def load_west_project_index() -> dict:
    '''Return a name -> Project map for all west manifest projects.'''
    manifest = Manifest.from_file()
    return {project.name: project for project in manifest.get_projects([])}


def validate_west_project(
    device: str,
    mode: str,
    west_project: str,
    west_projects: dict,
) -> str | None:
    '''Return an error message if a required west project is unavailable.'''
    if west_project not in west_projects:
        return (
            f'device_map.yml references west project {west_project!r} for '
            f'{device!r} mode {mode!r}, but it is not listed in west.yml'
        )

    project = west_projects[west_project]
    if not project.is_cloned():
        return (
            f'device {device!r} mode {mode!r} requires west project '
            f'{west_project!r}, which is not cloned in this workspace. '
            f'If you lack access, choose a different mode in dbuild_devices.conf. '
            f'If you should have access, run: west update'
        )

    module_yml = Path(project.abspath) / 'zephyr' / 'module.yml'
    if not module_yml.is_file():
        return (
            f'device {device!r} mode {mode!r} requires west project '
            f'{west_project!r}, but {module_yml} is missing'
        )

    return None


def snippet_has_board_overlay(snippet_path: Path, board: str) -> bool:
    '''Return True if the snippet has board overlay coverage for board.'''
    overlay = snippet_path / 'boards' / f'{board}.overlay'
    if not overlay.is_file():
        return False

    snippet_yml = snippet_path / 'snippet.yml'
    if not snippet_yml.is_file():
        return False

    with snippet_yml.open() as f:
        data = yaml.safe_load(f) or {}

    boards = data.get('boards', {})
    for key in boards:
        if key.split('/')[0] == board:
            return True

    return False


def resolve_snippets(
    devices_conf: dict[str, str],
    device_map: dict,
    board: str,
    west_projects: dict | None = None,
) -> list[str]:
    '''Resolve device modes to snippet names and validate support.'''
    board_name = board_short_name(board)
    snippets: list[str] = []
    errors: list[str] = []

    if west_projects is None:
        west_projects = load_west_project_index()

    for device, mode in devices_conf.items():
        if device not in device_map:
            errors.append(
                f'unknown device {device!r} in dbuild_devices.conf '
                f'(not defined in dbuild/device_map.yml)'
            )
            continue

        modes = device_map[device]
        if mode not in modes:
            valid = ', '.join(sorted(modes))
            errors.append(
                f'invalid mode {mode!r} for device {device!r} '
                f'(valid modes: {valid})'
            )
            continue

        entry = modes[mode]
        snippet_name = entry['snippet']
        snippet_path = snippet_dir(snippet_name)

        if not snippet_path.is_dir():
            errors.append(
                f'device {device!r} mode {mode!r} requires snippet '
                f'{snippet_name!r}, but {snippet_path} does not exist'
            )
            continue

        west_project = entry['west_project']
        west_error = validate_west_project(
            device, mode, west_project, west_projects,
        )
        if west_error:
            errors.append(west_error)
            continue

        if entry.get('board_overlay_required', False):
            if not snippet_has_board_overlay(snippet_path, board_name):
                supported = sorted(
                    p.stem
                    for p in (snippet_path / 'boards').glob('*.overlay')
                ) if (snippet_path / 'boards').is_dir() else []

                supported_msg = (
                    ', '.join(supported) if supported else '(none)'
                )
                errors.append(
                    f'device {device!r} mode {mode!r} (snippet {snippet_name!r}) '
                    f'is not supported on board {board_name!r}; '
                    f'supported boards: {supported_msg}'
                )
                continue

        snippets.append(snippet_name)

    if errors:
        raise ValueError('\n'.join(errors))

    return snippets


def split_west_and_cmake_extra(extra_args: list[str]) -> tuple[list[str], list[str]]:
    '''Split passthrough args into west build options and cmake options.

    West build options (e.g. -t run, -c, -o=...) must appear before the source
    directory. Only arguments after an explicit ``--`` are forwarded to cmake.
    '''
    if '--' in extra_args:
        sep = extra_args.index('--')
        return extra_args[:sep], extra_args[sep + 1:]
    return extra_args, []


def build_west_command(
    board: str,
    snippets: list[str],
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

    if cmake_extra:
        cmd.append('--')
        cmd.extend(cmake_extra)

    return cmd


class Dbuild(WestCommand):
    def __init__(self):
        super().__init__(
            'dbuild',
            'build with per-device modes from dbuild_devices.conf',
            description='Resolve per-device modes from dbuild_devices.conf and run west build.',
            accepts_unknown_args=True,
        )

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name,
            formatter_class=argparse.RawDescriptionHelpFormatter,
            description=self.description,
            epilog=(
                'dbuild is a thin wrapper around west build: it resolves device '
                'snippets, then runs west build with the same flags you would use '
                'directly. Any option not listed above is forwarded to west build '
                '(for example: -t run, -c, -o=-j8). Use -- only when passing raw '
                'cmake arguments, as with west build itself.\n\n'
                'Examples:\n'
                '  west dbuild -b nucleo_u575zi_q\n'
                '  west dbuild -b nucleo_u575zi_q -t run\n'
                '  west dbuild -b nucleo_u575zi_q -- -DCONFIG_LOG_DEFAULT_LEVEL=4'
            ),
        )

        parser.add_argument(
            '-b', '--board', required=True,
            help='board to build for (short name, e.g. nucleo_u575zi_q)',
        )
        parser.add_argument(
            '--devices-file',
            default=str(DEFAULT_DEVICES_FILE),
            help='path to dbuild_devices.conf (default: dbuild_devices.conf)',
        )
        parser.add_argument(
            '-m', '--device-map',
            default=str(DEFAULT_DEVICE_MAP),
            help='path to device_map.yml (default: dbuild/device_map.yml)',
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
        try:
            devices_conf = parse_devices_conf(Path(args.devices_file))
            device_map = load_device_map(Path(args.device_map))
            validate_device_map(device_map)
            snippets = resolve_snippets(devices_conf, device_map, args.board)
        except (FileNotFoundError, ValueError) as exc:
            self.die(str(exc))

        self.inf('Resolved snippets:', ' '.join(snippets))

        cmd = build_west_command(
            args.board,
            snippets,
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
