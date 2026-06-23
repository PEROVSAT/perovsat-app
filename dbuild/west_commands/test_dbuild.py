'''Tests for west dbuild command.

Run from the workspace root:
    pytest dbuild/west_commands/test_dbuild.py
'''

from __future__ import annotations

import sys
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest
import yaml

sys.path.insert(0, str(Path(__file__).parent))
import dbuild as db  # noqa: E402 — path manipulation required before import


# ── Fixtures ──────────────────────────────────────────────────────────────────

def make_west_project(*, cloned: bool = True, has_module_yml: bool = True, root: Path):
    '''Return a mock west Project with a real filesystem layout.'''
    module_yml = root / 'zephyr' / 'module.yml'
    if has_module_yml:
        module_yml.parent.mkdir(parents=True, exist_ok=True)
        module_yml.touch()

    proj = MagicMock()
    proj.is_cloned.return_value = cloned
    proj.abspath = str(root)
    return proj


def minimal_device_map(snippet: str = 'my-snippet') -> dict:
    return {
        'IMU': {
            'west_project': 'mpu6050-driver',
            'modes': {
                'public-mock': {
                    'snippet': snippet,
                    'kconfig_backend': 'CONFIG_PEROVSAT_MPU6050_BACKEND_PUBLIC_MOCK',
                }
            },
        }
    }


# ── board_short_name ──────────────────────────────────────────────────────────

class TestBoardShortName:
    def test_plain(self):
        assert db.board_short_name('nucleo_u575zi_q') == 'nucleo_u575zi_q'

    def test_strips_soc_qualifier(self):
        assert db.board_short_name('nucleo_u575zi_q/stm32u575xx') == 'nucleo_u575zi_q'

    def test_strips_revision(self):
        assert db.board_short_name('nucleo_u575zi_q@1.0') == 'nucleo_u575zi_q'

    def test_strips_qualifier_and_revision(self):
        assert db.board_short_name('nucleo_u575zi_q/stm32u575xx@1.0') == 'nucleo_u575zi_q'


# ── split_west_and_cmake_extra ────────────────────────────────────────────────

class TestSplitWestAndCmakeExtra:
    def test_empty(self):
        assert db.split_west_and_cmake_extra([]) == ([], [])

    def test_no_separator(self):
        assert db.split_west_and_cmake_extra(['-t', 'run']) == (['-t', 'run'], [])

    def test_separator_splits(self):
        west, cmake = db.split_west_and_cmake_extra(['-t', 'run', '--', '-DFOO=1'])
        assert west == ['-t', 'run']
        assert cmake == ['-DFOO=1']

    def test_separator_only(self):
        assert db.split_west_and_cmake_extra(['--']) == ([], [])

    def test_cmake_only(self):
        west, cmake = db.split_west_and_cmake_extra(['--', '-DA=1', '-DB=2'])
        assert west == []
        assert cmake == ['-DA=1', '-DB=2']


# ── load_dbuild_config ────────────────────────────────────────────────────────

class TestLoadDbuildConfig:
    def _write(self, tmp_path: Path, data: dict) -> Path:
        p = tmp_path / 'dbuild.yml'
        p.write_text(yaml.dump(data))
        return p

    def test_valid_config(self, tmp_path):
        p = self._write(tmp_path, {
            'selections': {'IMU': 'public-mock'},
            'devices': minimal_device_map(),
        })
        selections, device_map = db.load_dbuild_config(p)
        assert selections == {'IMU': 'public-mock'}
        assert 'IMU' in device_map

    def test_selections_normalized_to_lowercase(self, tmp_path):
        p = self._write(tmp_path, {
            'selections': {'IMU': 'Public-Mock'},
            'devices': {},
        })
        selections, _ = db.load_dbuild_config(p)
        assert selections['IMU'] == 'public-mock'

    def test_missing_file(self, tmp_path):
        with pytest.raises(FileNotFoundError, match='dbuild config not found'):
            db.load_dbuild_config(tmp_path / 'nonexistent.yml')

    def test_missing_selections_key(self, tmp_path):
        p = self._write(tmp_path, {'devices': {}})
        with pytest.raises(ValueError, match='"selections"'):
            db.load_dbuild_config(p)

    def test_missing_devices_key(self, tmp_path):
        p = self._write(tmp_path, {'selections': {'IMU': 'public-mock'}})
        with pytest.raises(ValueError, match='"devices"'):
            db.load_dbuild_config(p)

    def test_empty_selections_raises(self, tmp_path):
        p = self._write(tmp_path, {'selections': {}, 'devices': {}})
        with pytest.raises(ValueError, match='"selections"'):
            db.load_dbuild_config(p)


# ── validate_device_map ───────────────────────────────────────────────────────

class TestValidateDeviceMap:
    def test_valid(self):
        db.validate_device_map(minimal_device_map())

    def test_missing_west_project(self):
        dm = minimal_device_map()
        del dm['IMU']['west_project']
        with pytest.raises(ValueError, match='"west_project"'):
            db.validate_device_map(dm)

    def test_missing_snippet(self):
        dm = minimal_device_map()
        del dm['IMU']['modes']['public-mock']['snippet']
        with pytest.raises(ValueError, match='"snippet"'):
            db.validate_device_map(dm)

    def test_missing_kconfig_backend(self):
        dm = minimal_device_map()
        del dm['IMU']['modes']['public-mock']['kconfig_backend']
        with pytest.raises(ValueError, match='"kconfig_backend"'):
            db.validate_device_map(dm)

    def test_empty_modes(self):
        dm = minimal_device_map()
        dm['IMU']['modes'] = {}
        with pytest.raises(ValueError, match='"modes"'):
            db.validate_device_map(dm)

    def test_multiple_errors_reported_together(self):
        dm = {
            'IMU': {'modes': {'x': {}}},
            'MODEM': 'not-a-dict',
        }
        with pytest.raises(ValueError) as exc_info:
            db.validate_device_map(dm)
        msg = str(exc_info.value)
        assert 'IMU' in msg
        assert 'MODEM' in msg


# ── resolve_build_config ──────────────────────────────────────────────────────

class TestResolveBuildConfig:
    def test_resolves_snippet_and_kconfig(self, tmp_path):
        snippet_root = tmp_path / 'snippets'
        (snippet_root / 'my-snippet').mkdir(parents=True)

        proj = make_west_project(root=tmp_path / 'proj')

        with patch.object(db, 'SNIPPETS_ROOT', snippet_root):
            snippets, cmake_args = db.resolve_build_config(
                {'IMU': 'public-mock'},
                minimal_device_map(),
                'nucleo_u575zi_q',
                west_projects={'mpu6050-driver': proj},
            )

        assert snippets == ['my-snippet']
        assert cmake_args == ['-DCONFIG_PEROVSAT_MPU6050_BACKEND_PUBLIC_MOCK=y']

    def test_unknown_device(self):
        with pytest.raises(ValueError, match="unknown device 'UNKNOWN'"):
            db.resolve_build_config(
                {'UNKNOWN': 'public-mock'}, {}, 'board', west_projects={},
            )

    def test_invalid_mode(self):
        with pytest.raises(ValueError, match="invalid mode 'badmode'"):
            db.resolve_build_config(
                {'IMU': 'badmode'},
                minimal_device_map(),
                'board',
                west_projects={},
            )

    def test_missing_snippet_dir(self, tmp_path):
        snippet_root = tmp_path / 'snippets'
        snippet_root.mkdir()

        with patch.object(db, 'SNIPPETS_ROOT', snippet_root):
            with pytest.raises(ValueError, match='does not exist'):
                db.resolve_build_config(
                    {'IMU': 'public-mock'},
                    minimal_device_map(),
                    'board',
                    west_projects={},
                )

    def test_west_project_not_in_manifest(self, tmp_path):
        snippet_root = tmp_path / 'snippets'
        (snippet_root / 'my-snippet').mkdir(parents=True)

        with patch.object(db, 'SNIPPETS_ROOT', snippet_root):
            with pytest.raises(ValueError, match='not listed in west.yml'):
                db.resolve_build_config(
                    {'IMU': 'public-mock'},
                    minimal_device_map(),
                    'board',
                    west_projects={},
                )

    def test_west_project_not_cloned(self, tmp_path):
        snippet_root = tmp_path / 'snippets'
        (snippet_root / 'my-snippet').mkdir(parents=True)

        proj = make_west_project(cloned=False, root=tmp_path / 'proj')

        with patch.object(db, 'SNIPPETS_ROOT', snippet_root):
            with pytest.raises(ValueError, match='not cloned'):
                db.resolve_build_config(
                    {'IMU': 'public-mock'},
                    minimal_device_map(),
                    'board',
                    west_projects={'mpu6050-driver': proj},
                )

    def test_board_overlay_required_missing(self, tmp_path):
        snippet_root = tmp_path / 'snippets'
        (snippet_root / 'hw-snippet').mkdir(parents=True)

        dm = {
            'IMU': {
                'west_project': 'mpu6050-driver',
                'modes': {
                    'hardware': {
                        'snippet': 'hw-snippet',
                        'kconfig_backend': 'CONFIG_X',
                        'board_overlay_required': True,
                    }
                },
            }
        }
        proj = make_west_project(root=tmp_path / 'proj')

        with patch.object(db, 'SNIPPETS_ROOT', snippet_root):
            with pytest.raises(ValueError, match='not supported on board'):
                db.resolve_build_config(
                    {'IMU': 'hardware'}, dm, 'nucleo_u575zi_q',
                    west_projects={'mpu6050-driver': proj},
                )

    def test_multiple_devices_resolved(self, tmp_path):
        snippet_root = tmp_path / 'snippets'
        (snippet_root / 'imu-snippet').mkdir(parents=True)
        (snippet_root / 'modem-snippet').mkdir(parents=True)

        dm = {
            'IMU': {
                'west_project': 'imu-driver',
                'modes': {'mock': {'snippet': 'imu-snippet', 'kconfig_backend': 'CONFIG_IMU'}},
            },
            'MODEM': {
                'west_project': 'modem-driver',
                'modes': {'mock': {'snippet': 'modem-snippet', 'kconfig_backend': 'CONFIG_MODEM'}},
            },
        }

        imu_proj = make_west_project(root=tmp_path / 'imu')
        modem_proj = make_west_project(root=tmp_path / 'modem')

        with patch.object(db, 'SNIPPETS_ROOT', snippet_root):
            snippets, cmake_args = db.resolve_build_config(
                {'IMU': 'mock', 'MODEM': 'mock'},
                dm,
                'board',
                west_projects={'imu-driver': imu_proj, 'modem-driver': modem_proj},
            )

        assert set(snippets) == {'imu-snippet', 'modem-snippet'}
        assert '-DCONFIG_IMU=y' in cmake_args
        assert '-DCONFIG_MODEM=y' in cmake_args


# ── build_west_command ────────────────────────────────────────────────────────

class TestBuildWestCommand:
    def test_basic_structure(self):
        cmd = db.build_west_command(
            board='nucleo_u575zi_q',
            snippets=['mpu6050-public-mock'],
            cmake_kconfig_args=['-DCONFIG_X=y'],
            build_dir=None,
            pristine='always',
            extra_args=[],
        )
        assert cmd[:4] == ['west', 'build', '-b', 'nucleo_u575zi_q']
        assert '-S' in cmd
        assert 'mpu6050-public-mock' in cmd
        assert ['-p', 'always'] == cmd[cmd.index('-p'):cmd.index('-p') + 2]
        sep = cmd.index('--')
        cmake_section = cmd[sep + 1:]
        assert '-DCONFIG_X=y' in cmake_section
        assert '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON' in cmake_section

    def test_build_dir_flag(self):
        cmd = db.build_west_command('board', [], [], '/tmp/build', None, [])
        assert ['-d', '/tmp/build'] == cmd[cmd.index('-d'):cmd.index('-d') + 2]

    def test_no_pristine_omits_flag(self):
        cmd = db.build_west_command('board', [], [], None, None, [])
        assert '-p' not in cmd

    def test_multiple_snippets(self):
        cmd = db.build_west_command('board', ['s1', 's2'], [], None, None, [])
        s_indices = [i for i, x in enumerate(cmd) if x == '-S']
        assert len(s_indices) == 2
        assert cmd[s_indices[0] + 1] == 's1'
        assert cmd[s_indices[1] + 1] == 's2'

    def test_cmake_extra_via_separator(self):
        cmd = db.build_west_command('board', [], [], None, None, ['--', '-DFOO=bar'])
        sep = cmd.index('--')
        assert '-DFOO=bar' in cmd[sep + 1:]

    def test_west_extra_before_source_dir(self):
        cmd = db.build_west_command('board', [], [], None, None, ['-t', 'run'])
        sep = cmd.index('--')
        assert '-t' in cmd[:sep]
        assert 'run' in cmd[:sep]
