# dbuild

Per-device build modes for PerovSat flight software. `west dbuild` reads
`dbuild_devices.conf`, resolves each device to a Zephyr snippet and backend
Kconfig symbol, validates board support, and runs `west build` with the
correct `-S` and `-D` flags.

Application code does not change between modes — only devicetree aliases and
Kconfig selections differ per snippet.

## Quick start

1. Edit [dbuild_devices.conf](../dbuild_devices.conf) at the app root:

   ```
   IMU=public-mock
   ```

2. Build for your board:

   ```sh
   west dbuild -b nucleo_u575zi_q
   ```

3. Flash as usual (uses the existing build directory):

   ```sh
   west flash
   ```

Preview the resolved command without building:

```sh
west dbuild -b nucleo_u575zi_q --dry-run
```

Build and run under QEMU (or your board's run target) the same way as `west build`:

```sh
west dbuild -b qemu_cortex_m3 -t run
```

## How it works

```
dbuild_devices.conf  →  device_map.yml  →  snippets/*  →  west build -S ... -- -D...
```

- **dbuild_devices.conf** — which mode each device uses (`public-mock`, `library-mock`,
  `simulation`, or `hardware`)
- **device_map.yml** — maps each device to a west driver project and per-mode snippet
  plus backend Kconfig symbol
- **snippets/** — atomic per-mode Zephyr snippets (overlay + base Kconfig)
- **west driver repos** — Zephyr modules listed in `west.yml` (e.g. `mpu6050-driver`)

`west dbuild` validates before CMake runs:

- Every device in the conf file is defined in `device_map.yml`
- Every device defines `west_project` and every mode defines `snippet` and
  `kconfig_backend`
- Every resolved snippet directory exists under `snippets/`
- Every required west project is cloned and contains `zephyr/module.yml`
- For modes with `board_overlay_required`, `snippets/<snippet>/boards/<board>.overlay`
  exists and the snippet's `snippet.yml` lists that board

dbuild injects each mode's `kconfig_backend` as a CMake `-D` flag so the driver's
CMake/Kconfig choice block selects the correct backend at build time.

## IMU modes

| Mode | Backend | Description |
|------|---------|-------------|
| `public-mock` | `CONFIG_PEROVSAT_MPU6050_BACKEND_PUBLIC_MOCK` | Static data from the driver API, no device library |
| `library-mock` | `CONFIG_PEROVSAT_MPU6050_BACKEND_LIBRARY_MOCK` | Static register map via the device library |
| `simulation` | `CONFIG_PEROVSAT_MPU6050_BACKEND_SIMULATION` | Socket communication for SITL |
| `hardware` | `CONFIG_PEROVSAT_MPU6050_BACKEND_HARDWARE` | Real bus I/O (e.g. I2C) |

## Adding a new device

1. Add the driver as a west project in `west.yml`.
2. Create atomic snippets under `snippets/` (e.g. `sun-sensor-public-mock/`,
   `sun-sensor-hardware/`).
3. Add the device to [device_map.yml](device_map.yml) with `west_project` and
   per-mode `snippet`, `kconfig_backend`, and `board_overlay_required`.
4. Add a line to [dbuild_devices.conf](../dbuild_devices.conf).

See the comments in `device_map.yml` for the expected schema.

## Adding board support for hardware

For a hardware or simulation snippet, add an overlay at:

```
snippets/<snippet-name>/boards/<board>.overlay
```

and register it in that snippet's `snippet.yml` under `boards:`.

Example for `mpu6050-hardware` on `nucleo_u575zi_q`:

```yaml
boards:
  nucleo_u575zi_q/stm32u575xx:
    append:
      EXTRA_DTC_OVERLAY_FILE: boards/nucleo_u575zi_q.overlay
```

`west dbuild` matches on the board short name (`nucleo_u575zi_q`), so you
can pass `-b nucleo_u575zi_q` without the SoC qualifier.

## Files

| File | Purpose |
|------|---------|
| `dbuild_devices.conf` | Per-device mode selection (committed) |
| `dbuild/device_map.yml` | Device → mode → snippet and backend mapping |
| `dbuild/west-commands.yml` | Registers `west dbuild` with west |
| `dbuild/west_commands/dbuild.py` | Command implementation |
| `snippets/` | Atomic Zephyr snippets per device/mode |

## Extra build options

`west dbuild` is a thin wrapper around `west build`. It resolves device snippets
and backend Kconfig symbols, then forwards any flag it does not recognize
directly to `west build` — no extra `--` is needed for normal west options like
`-t run`, `-c`, or `-o=-j8`.

`west dbuild` defaults to `-p always` so snippet or mode changes always produce
a clean build (and a matching binary for `west flash`). Use `-p never` for
faster incremental rebuilds when you know the device configuration is unchanged.

Use `--` only when passing additional raw cmake arguments, matching `west build`
itself:

```sh
west dbuild -b nucleo_u575zi_q -- -DCONFIG_LOG_DEFAULT_LEVEL=4
```

dbuild-specific options use long forms where short flags would collide with
`west build` (`--devices-file` instead of `-f`, `--dry-run` instead of `-n`).

After adding or changing the west extension, run `west update` once so west
discovers the new command.
