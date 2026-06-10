# dbuild

Per-device build modes for PerovSat flight software. `west dbuild` reads
`dbuild_devices.conf`, resolves each device to a Zephyr snippet, validates
board support, and runs `west build` with the correct `-S` flags.

Application code does not change between mock and hardware modes — only
devicetree aliases and Kconfig selections differ per snippet.

## Quick start

1. Edit [dbuild_devices.conf](../dbuild_devices.conf) at the app root:

   ```
   IMU=mock
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

## How it works

```
dbuild_devices.conf  →  device_map.yml  →  snippets/*  →  west build -S ...
```

- **dbuild_devices.conf** — which mode each device uses (`mock` or `hardware`)
- **device_map.yml** — maps `(device, mode)` to a snippet and west driver project
- **snippets/** — atomic per-device Zephyr snippets (overlay + Kconfig)
- **west driver repos** — Zephyr modules listed in `west.yml` (e.g. `imu-mock-driver`)

`west dbuild` validates before CMake runs:

- Every device in the conf file is defined in `device_map.yml`
- Every mode in `device_map.yml` defines `snippet` and `west_project`
- Every resolved snippet directory exists under `snippets/`
- Every required west project is cloned and contains `zephyr/module.yml`
- For modes with `board_overlay_required`, `snippets/<snippet>/boards/<board>.overlay`
  exists and the snippet's `snippet.yml` lists that board

## Adding a new device

1. Add the driver as a west project in `west.yml`.
2. Create atomic snippets under `snippets/` (e.g. `sun-z-mock/`, `sun-z-hw/`).
3. Add the device to [device_map.yml](device_map.yml) with `snippet`, `west_project`,
   and `board_overlay_required` for each mode.
4. Add a line to [dbuild_devices.conf](../dbuild_devices.conf).

See the comments in `device_map.yml` for the expected schema.

## Adding board support for hardware

For a hardware snippet, add an overlay at:

```
snippets/<snippet-name>/boards/<board>.overlay
```

and register it in that snippet's `snippet.yml` under `boards:`.

Example for `imu-hw` on `nucleo_u575zi_q`:

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
| `dbuild/device_map.yml` | Device → mode → snippet mapping |
| `dbuild/west-commands.yml` | Registers `west dbuild` with west |
| `dbuild/west_commands/dbuild.py` | Command implementation |
| `snippets/` | Atomic Zephyr snippets per device/mode |

## Extra build options

`west dbuild` defaults to `-p always` so snippet or mode changes always produce
a clean build (and a matching binary for `west flash`). Use `-p never` for
faster incremental rebuilds when you know the device configuration is unchanged.

Pass additional `west build` arguments after `--`:

```sh
west dbuild -b nucleo_u575zi_q -- -DCONFIG_LOG_DEFAULT_LEVEL=4
```

After adding or changing the west extension, run `west update` once so west
discovers the new command.
