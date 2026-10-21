# DIELTECH srl - LabJack U3-LV Modbus TCP Gateway

A native C gateway that exposes a USB-connected **LabJack U3-LV** to Modbus TCP clients on Linux and Raspberry Pi.

The gateway forwards official **UD Modbus** requests to the U3 firmware using the LabJack Exodriver. A separate DIELTECH register block provides configuration staging, explicit APPLY, and gateway diagnostics.

> **Documentation target:** gateway map v2, including the TCP idle-connection fix. Do not combine this documentation with the earlier easy-API backend, whose custom registers occupied `7100-7115`, `7199`, and `7400-7401`.

## Contents

- [Features](#features)
- [Architecture](#architecture)
- [Requirements](#requirements)
- [Build and run](#build-and-run)
- [Client configuration](#client-configuration)
- [Register map overview](#register-map-overview)
- [Staging and APPLY](#staging-and-apply)
- [Inspection utility](#inspection-utility)
- [Examples](#examples)
- [Systemd installation](#systemd-installation)
- [Diagnostics and troubleshooting](#diagnostics-and-troubleshooting)
- [Validation status](#validation-status)
- [Safety and limitations](#safety-and-limitations)
- [References](#references)

## Features

- C implementation, with no Python runtime needed.
- Native UD Modbus forwarding: official register meanings are implemented by the device firmware, not reinterpreted in the gateway.
- Access to analog inputs, DAC outputs, DIO state/direction, timer/counter registers and supported device information.
- Separate configuration staging with explicit load and APPLY commands.
- No automatic I/O reconfiguration at gateway startup.
- Persistent TCP connection during ordinary idle periods in the corrected `main.c`.
- Diagnostic client that avoids digital reads on analog pins and analog reads on digital pins when the required mode information is available.
- Explicit Modbus exceptions instead of simulated success for unsupported functionality.

This is not a claim that every register in LabJack's multi-device UD table exists on the U3. Firmware support and the table's `x`, `n`, and `f` annotations still apply.

## Architecture

```text
Modbus TCP client / PLC / SCADA
                |
                | TCP port 5020
                v
       DIELTECH gateway (C)
         |                 |
         | official UD     | DIELTECH extension
         | requests        | 45000-45031
         v                 v
  Exodriver / USB       staging + diagnostics
         |
         v
  LabJack U3 firmware
```

USB requests receive the two-byte zero prefix required by UD Modbus. TCP clients must not insert this prefix themselves. LJSocket is not used by this gateway.

## Requirements

### Hardware

- LabJack U3-LV with native UD Modbus support.
- LabJack documents this support for U3 hardware **1.30**, firmware **1.05 or later**. Check the actual device revision; older U3 hardware revisions are not equivalent.
- Linux computer or Raspberry Pi with a USB host port.

### Software

- GCC and GNU make.
- LabJack Exodriver source tree and `liblabjackusb`.
- libusb development files and the runtime libraries needed by the Exodriver.

The Makefile uses a configurable Exodriver path. The original project location was:

```text
/home/benjiworld/lj/exodriver-master
```

## Repository layout

```text
main.c                     TCP server, framing and connection handling
modbus_u3.c                Native UD forwarding and custom staging backend
modbus_u3.h                v2 context and mb_request() interface
modbus_inspect_all.c        Read-only inspection utility
Makefile                   Builds server and inspector
modbus-u3.service          Optional systemd service
README.md                  This guide
REGISTER-MAP.md            Detailed data sizes, I/O channels and registers
```

`u3.c`, `u3.h`, and `labjackusb.h` are provided by the external Exodriver tree. The old `u3_easy_wrap.c` is not linked by the v2 gateway.

## Build and run

```bash
make clean
make EXODRIVER=/home/benjiworld/lj/exodriver-master
```

Both programs are built:

```text
modbus_u3_server
modbus_inspect_all
```

Start the server:

```bash
sudo ./modbus_u3_server
```

Expected startup text includes:

```text
DIELTECH srl | U3 native UD Modbus gateway | map v2
Modbus TCP server listening on port 5020
```

The exact banner may differ in the corrected `main.c`.

Use `make clean` to remove build products. The Makefile suppresses `-Wunused-parameter` only for the upstream `u3.c`, while retaining `-Wall -Wextra` for project code.

Root is not intrinsically required by TCP port 5020; it may be required by the current USB permissions. Suitable udev rules can allow a dedicated non-root account to access the U3.

## Client configuration

| Setting | Value |
|---|---|
| Protocol | Modbus TCP, not RTU-over-TCP |
| Host | IP address of the Linux/Raspberry Pi gateway |
| TCP port | `5020` |
| Unit ID used in project tests | `1` |
| Address convention | Raw, zero-based |
| Read functions | FC03 and FC04 |
| Write functions | FC06 and FC16 (`0x10`) |
| Suggested initial response timeout | 3000 ms; adjust to the application |
| Suggested initial polling period | 1000 ms |

Do not enter traditional `4xxxx` notation. A client using one-based display numbers may show raw register `45025` as `45026`; verify its address-base setting rather than changing server constants.

The Unit ID is forwarded to the U3. The LabJack documentation describes echo behavior and also recommends `0xFF` in its protocol table. Unit ID `1` worked in the reported project tests.

### TCP behavior

The corrected server distinguishes idle time from an incomplete request:

- No bytes of a new MBAP header: wait again after the receive timeout; do not close the connection.
- Partially received MBAP header: timeout closes the connection.
- Header received but PDU incomplete: timeout closes the connection.
- Client closes or resets the connection: release the socket and accept a new client.

The configured socket timeout is 10 seconds per blocking transfer operation, not an absolute deadline for an entire request. The TCP server handles **one client at a time**. An idle connected client can prevent another client from being served. Disconnect Modbux before running the inspector.

## Register map overview

See [REGISTER-MAP.md](REGISTER-MAP.md) for the complete U3-LV port-by-port project reference.

| Function | Raw addresses | Format per value |
|---|---|---|
| AIN0-AIN15 | `0-31` | Float32, 32 bit / 2 registers |
| DAC0-DAC1 | `5000-5003` | Float32, 32 bit / 2 registers |
| DIO0-DIO19 state | `6000-6019` | UInt16, 16 bit / 1 register |
| DIO0-DIO19 direction | `6100-6119` | UInt16, 16 bit / 1 register |
| Timer clock base | `7000-7001` | UInt32, 32 bit / 2 registers |
| Timer divisor | `7002-7003` | UInt32, 32 bit / 2 registers |
| Timer0/1 configuration | `7100-7103` | Raw UInt32, 32 bit / 2 registers per timer |
| Timer0/1 read/reset | `7200-7203` | 32 bit / 2 registers per timer; mode-dependent interpretation |
| Counter0/1 read/reset | `7300-7303` | UInt32, 32 bit / 2 registers per counter |
| DIELTECH staging | `45000-45012` | Mixed 16/32-bit fields |
| DIELTECH command | `45020` | Write-only UInt16 |
| DIELTECH diagnostics | `45021-45025` | UInt16, 16 bit / 1 register each |
| Timer/counter allocation | `50500-50502` | UInt16, 16 bit / 1 register each |
| FIO/EIO analog masks | `50590-50591` | UInt16, 16 bit / 1 register each |
| Native detailed error | `55000` | UInt16, 16 bit / 1 register |
| Product ID | `65000` | UInt16, 16 bit / 1 register |

Every Modbus register is 16 bits. For 32-bit values, high word comes first. Bytes inside each register use network byte order.

## Staging and APPLY

Official UD writes are **immediate**. Only writes to the DIELTECH staging block are deferred.

### 1. Load a hardware snapshot

```text
FC06: raw address 45020, value 2
```

Check:

```text
FC03: raw address 45022, quantity 2
45022 = 0  command completed without exception
45023 = 1  staging snapshot has been loaded
```

The load command reads masks, allocation, clock settings and both timer configuration pairs. It commits the staging snapshot only if all required reads succeed. A device rejecting one of these reads will cause the load command to fail.

### 2. Edit the staged fields

Read/write `45000-45012` according to REGISTER-MAP.md. Use FC16 for a complete 32-bit field.

Staged timer configuration is the raw official 32-bit configuration image. It is **not** Float32 and is not the old custom pair of separate mode and initial-value fields. Preserve the loaded raw value unless its firmware encoding has been verified for the intended mode.

### 3. Apply

```text
FC06: raw address 45020, value 42330  # 0xA55A
```

### 4. Verify

```text
FC03: raw address 45021, quantity 5
```

Then read the actual official registers affected by the operation. Readback of staging alone is not proof that hardware changed.

### Critical properties

- APPLY is sequential, **not atomic**, and has no rollback.
- It disables timers/counters before applying allocation/configuration and re-enabling requested resources.
- A failed APPLY may leave hardware partly changed, including resources disabled.
- The staging allocation validator conservatively restricts timer/counter pins to FIO and checks the analog-mask conflict.
- `45023 = 1` means a snapshot was loaded, not that it is continuously synchronized with hardware or that all subsequent edits are valid.
- Direct UD writes, other tools, or a device reset can make staging stale. Reload before starting a new configuration change.
- Staging and diagnostics are process memory, not persistent configuration storage.
- Restarting the gateway does not deliberately reset the U3 I/O. Device reset or power cycling is a separate event.

## Inspection utility

Disconnect other clients, then run:

```bash
./modbus_inspect_all
# or:
./modbus_inspect_all 192.168.1.50 5020
```

The inspector:

- Reads analog masks before deciding which AIN/DIO channels to query.
- Prints raw words and decoded Float32 or UInt32 values.
- Reads native direction registers, not the legacy server direction shadow.
- Reads live timers/counters only when their enable registers indicate they are active.
- Prints staging words and gateway diagnostics.
- Never writes commands, resets, or outputs, and does not read `45020`.

A displayed zero on a disabled or skipped resource must not be interpreted as a physical measurement. Unconnected AIN values are not calibration references. DAC register readback is not a substitute for measuring the output with an external instrument.

## Examples

### Identify the gateway and device

```text
FC03, address 45025, quantity 1  -> 2 (custom map version)
FC03, address 65000, quantity 1  -> 3 (U3 product ID in reported test)
```

### Read AIN0

```text
FC03, address 0, quantity 2
Interpret as IEEE-754 Float32, high word first, volts.
```

### Write DAC0 to 1.0 V

```text
FC16, address 5000, quantity 2
Words: 0x3F80, 0x0000
```

Use a safe disconnected test load and observe U3 electrical limits. Do not connect a DAC to a U3-LV input without ensuring the voltage is within the input's allowed range.

### Make FIO0 digital using staging

On a bench, with the consequences of changing that pin understood:

```text
FC06, address 45020, value 2      # load snapshot
# Read and verify 45023 == 1.
# Read 45000 and clear ONLY bit 0; preserve other mask bits.
FC06, address 45000, value <modified FIO mask>
FC06, address 45020, value 42330  # APPLY
# Verify diagnostics, then read 50590 and check bit 0 == 0.
```

This changes pin mode; it does not by itself request an output HIGH. Output state/direction are separate native registers.

### Reserve FIO4 for Timer0

Required staged conditions include pin offset `4`, number of timers `1`, and bit 4 of the FIO analog mask cleared. If the initial mask is `0xFF`, the new mask is `0xEF` (239). Choose a supported native clock and timer configuration before APPLY. Do not copy the former easy-API clock index or Float32 timer initial-value scheme into the native raw configuration.

Actual timer output and counter-reset behavior still require device tests; no universal frequency or mode is implied by this example.

## Systemd installation

With the supplied v2 Makefile and service template:

```bash
sudo make install
sudo make systemd
sudo systemctl enable --now modbus-u3
```

Inspect service state and logs:

```bash
systemctl status modbus-u3
journalctl -u modbus-u3 -f
```

To deploy a rebuilt server:

```bash
sudo systemctl stop modbus-u3
make
sudo make install
sudo systemctl start modbus-u3
```

`make` alone does not replace `/usr/local/bin/modbus_u3_server`. The supplied service expects that path; if `BINDIR` is overridden, update `ExecStart` too.

Before uninstalling:

```bash
sudo systemctl disable --now modbus-u3
sudo make uninstall
```

## Diagnostics and troubleshooting

| Symptom | Meaning / next action |
|---|---|
| Reading `45020` returns exception 02 | Expected: command is write-only. Use FC06. |
| A read range including `45020` fails | Split reads; read `45021-45025`, not `45020-45025`. |
| `45023 = 0` and staged values are zero | No successful snapshot load has occurred. |
| `45021 = 0`, `45024 = 0` | Not proof of APPLY success; fields start at zero. |
| `45022` is nonzero | Stored Modbus exception code, not a low-level LabJack error code. |
| `45022` remains nonzero after successful ordinary reads | It is historical; ordinary successful reads do not necessarily clear it. |
| Native command fails with exception 02 | Device rejected an address; check firmware support and request boundaries. |
| Exception 03 | Invalid value, shape, length or quantity; check 32-bit pair and write limit. |
| Exception 04 | Gateway USB transport/response problem, or a device exception; inspect logs. |
| `TCP receive timeout: 0/7 bytes` causes reconnect | Old TCP idle handling is still in use; install corrected `main.c` and restart the correct binary. |
| Another client connects but receives no replies | Single-client server is already occupied. |
| Bind fails | Another process is listening on port 5020 or the address cannot be bound. |

Native `55000` and custom `45022` are different diagnostics. Read both when investigating errors.

## Validation status

Reported hardware observations during project testing:

- TCP and native USB reads responded.
- Product ID `65000` returned `3`.
- AIN0-AIN15 returned Float32 readings.
- Native CIO input states and directions were readable.
- Clock, divisor, allocation and analog masks were readable.
- `55000` returned zero in the reported inspection.
- Staging load succeeded: `45022 = 0`, `45023 = 1`, with staged fields matching read hardware values.

These observations do not establish full conformance or measurement accuracy. Full APPLY sequencing, all timer modes, counter resets, DAC accuracy, fault recovery and long-running operation have not all been demonstrated in the supplied logs. Validate them on a safe bench before deployment.

The source files mounted when this documentation was prepared were from the older backend. This guide targets the v2 code and corrected main.c supplied in the conversation, not those legacy attachments. Check the repository contents before publishing.

## Safety and limitations

- **No authentication or encryption.** Restrict TCP 5020 to trusted hosts with a firewall, isolated network or VPN.
- The server binds to all IPv4 interfaces in the supplied main.c.
- Official writes are forwarded, including potentially sensitive operations. The gateway does not provide a comprehensive firmware-operation allowlist.
- Never scan registers by writing arbitrary values. Flash, bootloader, restart and calibration operations can be destructive.
- One client at a time; no simultaneous multi-client service.
- TCP read limit: 125 registers, split into USB requests of at most 26 registers. Multi-block reads are not atomic snapshots.
- FC16 write limit: 24 registers per request. Larger writes return exception 03; the gateway does not split writes.
- Coils/discrete-input functions FC01/FC02 are not implemented by this v2 protocol interface; use official holding-register access.
- The gateway relies on the firmware's behavior, including entries marked partially tested or accepted without implementation in the official table.
- No automatic configuration persistence, rollback or application-level watchdog.
- Reconnecting after USB removal may require restarting the gateway; automatic USB recovery is not implemented.

## License

No project license is assigned by this documentation. Before publishing, the repository owner should choose an appropriate license and add a `LICENSE` file. LabJack components retain their upstream license terms; do not imply that this documentation relicenses them.

## References

- [Official UD Modbus map (deprecated)](https://support.labjack.com/docs/ud-modbus-old-deprecated)
- [LabJack Exodriver](https://github.com/labjack/exodriver)
- [LabJack support](https://support.labjack.com/)

UD Modbus is deprecated by LabJack. This gateway intentionally targets that legacy U3 interface; it is not a T-series Modbus implementation.
