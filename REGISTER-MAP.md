# REGISTER-MAP - DIELTECH U3-LV Gateway v2

## Scope and notation

Complete port-by-port reference for the U3-LV I/O and DIELTECH v2 extension described by this project. It is not a claim that every row in LabJack's multi-device UD map is supported by U3.

- All addresses are raw, zero-based Modbus addresses.
- Every holding register is 16 bits. `32 / 2` means a 32-bit value occupying two registers; `16 / 1` means one 16-bit register.
- R = read, W = write; R/W = both. Device access follows firmware support.
- FC03/FC04 read, FC06 writes one register, FC16 writes multiple registers.
- For 32-bit writes use FC16 with both words together.
- High word first; bytes within each register are network byte order.
- `UInt32 = (high_word << 16) | low_word`; Float32 reinterprets those same bits as IEEE-754 single precision.
- Timer values can be signed or unsigned and mode-dependent; raw bit representation is not an engineering-unit conversion.

## Network and logical ports

| Port/interface | Purpose                                                         |
| -------------- | --------------------------------------------------------------- |
| TCP 5020       | Client connection to gateway; configurable in main.c            |
| USB            | Exodriver communication with the U3; no additional TCP listener |
| Unit ID        | 1 used in project tests; ID is forwarded to firmware            |

One TCP client is served at a time. The corrected TCP server permits ordinary idle connections to remain open.

## Every flexible and digital I/O channel

State/direction entries below are each **16 bit / 1 register**, native R/W subject to firmware behavior. Analog entries are each **Float32, 32 bit / 2 registers, R**, in volts.

FIO/EIO analog selection is a bit in a 16-bit mask, not a separate register per pin. A physical pin cannot simultaneously be analog and general digital/timer-counter I/O. This table gives logical signal names, not DB connector pin numbers or an electrical wiring specification.

| Physical signal | DIO index | Analog input | AIN register pair | State HR (16/1) | Direction HR (16/1) | Analog mask and bit |
| --------------- | ---------:| ------------ | ----------------- | ---------------:| -------------------:| ------------------- |
| FIO0            | 0         | AIN0         | 0-1               | 6000            | 6100                | 50590, bit 0        |
| FIO1            | 1         | AIN1         | 2-3               | 6001            | 6101                | 50590, bit 1        |
| FIO2            | 2         | AIN2         | 4-5               | 6002            | 6102                | 50590, bit 2        |
| FIO3            | 3         | AIN3         | 6-7               | 6003            | 6103                | 50590, bit 3        |
| FIO4            | 4         | AIN4         | 8-9               | 6004            | 6104                | 50590, bit 4        |
| FIO5            | 5         | AIN5         | 10-11             | 6005            | 6105                | 50590, bit 5        |
| FIO6            | 6         | AIN6         | 12-13             | 6006            | 6106                | 50590, bit 6        |
| FIO7            | 7         | AIN7         | 14-15             | 6007            | 6107                | 50590, bit 7        |
| EIO0            | 8         | AIN8         | 16-17             | 6008            | 6108                | 50591, bit 0        |
| EIO1            | 9         | AIN9         | 18-19             | 6009            | 6109                | 50591, bit 1        |
| EIO2            | 10        | AIN10        | 20-21             | 6010            | 6110                | 50591, bit 2        |
| EIO3            | 11        | AIN11        | 22-23             | 6011            | 6111                | 50591, bit 3        |
| EIO4            | 12        | AIN12        | 24-25             | 6012            | 6112                | 50591, bit 4        |
| EIO5            | 13        | AIN13        | 26-27             | 6013            | 6113                | 50591, bit 5        |
| EIO6            | 14        | AIN14        | 28-29             | 6014            | 6114                | 50591, bit 6        |
| EIO7            | 15        | AIN15        | 30-31             | 6015            | 6115                | 50591, bit 7        |
| CIO0            | 16        | -            | -                 | 6016            | 6116                | Digital only        |
| CIO1            | 17        | -            | -                 | 6017            | 6117                | Digital only        |
| CIO2            | 18        | -            | -                 | 6018            | 6118                | Digital only        |
| CIO3            | 19        | -            | -                 | 6019            | 6119                | Digital only        |

Direction: `0` input, `1` output. State: `0` LOW, `1` HIGH. Use native register semantics and do not assume the legacy direction shadow still exists. LabJack marks U3 direction writes as implemented but not fully tested in the cited table.

## Analog outputs and timer/counter channels

| Channel/function     | Raw addresses | Bits / registers | Type         | Access | Notes                                                                |
| -------------------- | ------------- | ---------------- | ------------ | ------ | -------------------------------------------------------------------- |
| DAC0                 | 5000-5001     | 32 / 2           | Float32 V    | R/W    | Native DAC value; use external measurement for physical verification |
| DAC1                 | 5002-5003     | 32 / 2           | Float32 V    | R/W    | As above                                                             |
| Timer clock base     | 7000-7001     | 32 / 2           | UInt32       | R/W    | Native clock index; not necessarily the easy-API enum                |
| Timer divisor        | 7002-7003     | 32 / 2           | UInt32       | R/W    | Native interpretation, including zero, follows firmware              |
| Timer0 configuration | 7100-7101     | 32 / 2           | Raw UInt32   | R/W    | Preserve native packed image; not Float32                            |
| Timer1 configuration | 7102-7103     | 32 / 2           | Raw UInt32   | R/W    | Same                                                                 |
| Timer0 read/reset    | 7200-7201     | 32 / 2           | UInt32/Int32 | R/W    | Mode-dependent; verify native write/reset semantics before use       |
| Timer1 read/reset    | 7202-7203     | 32 / 2           | UInt32/Int32 | R/W    | Same                                                                 |
| Counter0 read/reset  | 7300-7301     | 32 / 2           | UInt32       | R/W    | Verify native reset operation on bench                               |
| Counter1 read/reset  | 7302-7303     | 32 / 2           | UInt32       | R/W    | Same                                                                 |

U3 has two timers and two counters, not the four timer slots shown for some devices in the generic UD table. Allocation starts at the configured timer/counter offset and depends on enabled resources. Observe native constraints, including clock/counter compatibility, in the U3 documentation.

## Allocation, analog masks and port-level native registers

| Raw address | Bits / registers per value | Access | Description                                                   |
| ----------- | -------------------------- | ------ | ------------------------------------------------------------- |
| 50500       | 16 / 1                     | R/W    | Timer/counter pin offset                                      |
| 50501       | 16 / 1                     | R/W    | Number of timers enabled, not independent enable bits         |
| 50502       | 16 / 1                     | R/W    | Counter mask: bit 0 Counter0, bit 1 Counter1                  |
| 50590       | 16 / 1                     | R/W    | FIO analog mask: bits 0-7 = FIO0-7, 1 analog / 0 digital      |
| 50591       | 16 / 1                     | R/W    | EIO analog mask: bits 0-7 = EIO0-7, 1 analog / 0 digital      |
| 6700        | 16 / 1                     | R/W    | FIO states; high byte write mask, low byte states             |
| 6701        | 16 / 1                     | R/W    | EIO states; high byte write mask, low byte states             |
| 6702        | 16 / 1                     | R/W    | CIO states; high byte write mask, low byte states             |
| 6750        | 16 / 1                     | R/W    | FIO directions; high byte write mask, low byte direction bits |
| 6751        | 16 / 1                     | R/W    | EIO directions; same format                                   |
| 6752        | 16 / 1                     | R/W    | CIO directions; same format                                   |
| 6800        | 16 / 1                     | R/W    | Raw single IO state; native encoding, not a DIO channel array |
| 6900        | 16 / 1                     | R/W    | Raw single IO direction; native encoding                      |

The grouped/raw IO operations above are marked `n` for U3 in the official table (implemented, not fully tested by LabJack). MIO entries in that multi-device table must not be treated as extra U3-LV ports.

## Additional official AIN settings

For external channel `n = 0..15`:

| Function             | Address formula | Bits / registers per channel | U3 table status                                |
| -------------------- | --------------- | ---------------------------- | ---------------------------------------------- |
| AIN resolution       | 1000+n          | 16 / 1                       | Read implemented; write marked f               |
| AIN range            | 1500+n          | 16 / 1                       | Read implemented; write marked f               |
| AIN settling         | 2000+n          | 16 / 1                       | Read implemented; write marked f               |
| AIN options          | 2500+n          | 16 / 1                       | Read indicated; do not assume writes supported |
| AIN negative channel | 3000+n          | 16 / 1                       | R/W indicated                                  |

`f` means accepted without real implementation according to LabJack; a successful write is not evidence that hardware changed. Additional special AIN channel addresses may exist in firmware and are not physical external ports.

## Native diagnostics and identification

| Raw addresses | Bits / registers | Access | Meaning                                               |
| ------------- | ---------------- | ------ | ----------------------------------------------------- |
| 55000         | 16 / 1           | R      | UD detailed error                                     |
| 65000         | 16 / 1           | R      | Product ID; 3 observed for U3                         |
| 65001-65002   | 32 / 2           | R      | Serial number                                         |
| 65003         | 16 / 1           | R      | Local ID                                              |
| 65004         | 16 / 1           | R      | Hardware version, native encoding                     |
| 65005         | 16 / 1           | R      | Hardware options                                      |
| 65006         | 16 / 1           | R      | Firmware version, official spf encoding (not Float32) |
| 65007         | 16 / 1           | R      | Bootloader version, official spf encoding             |

The gateway forwards other official addresses too; firmware support decides their behavior. Stream, serial buses, flash, calibration and other-device register families are not asserted as tested project features here. Use the upstream table for those specialist operations; do not probe them with arbitrary writes.

## DIELTECH extension: staged configuration

Block `45000-45031` is a project-local extension absent from the cited UD table. It is **not a range assigned or guaranteed reserved by LabJack**. It must be rechecked if porting to other devices or maps.

| Addresses   | Bits / registers | Access     | Type        | Description                 | Official destination |
| ----------- | ---------------- | ---------- | ----------- | --------------------------- | -------------------- |
| 45000       | 16 / 1           | R/W staged | UInt16 mask | FIO analog mask             | 50590                |
| 45001       | 16 / 1           | R/W staged | UInt16 mask | EIO analog mask             | 50591                |
| 45002       | 16 / 1           | R/W staged | UInt16      | Timer/counter offset        | 50500                |
| 45003       | 16 / 1           | R/W staged | UInt16      | Number timers, 0-2          | 50501                |
| 45004       | 16 / 1           | R/W staged | UInt16 mask | Counter enable mask, 0-3    | 50502                |
| 45005-45006 | 32 / 2           | R/W staged | UInt32      | Native clock base           | 7000-7001            |
| 45007-45008 | 32 / 2           | R/W staged | UInt32      | Native clock divisor        | 7002-7003            |
| 45009-45010 | 32 / 2           | R/W staged | Raw UInt32  | Native Timer0 configuration | 7100-7101            |
| 45011-45012 | 32 / 2           | R/W staged | Raw UInt32  | Native Timer1 configuration | 7102-7103            |

Each word of a staged 32-bit field is addressable, but write the complete pair with FC16. No hardware change occurs until APPLY.

### Command and diagnostics

| Address | Bits / registers | Access | Description                                                           |
| ------- | ---------------- | ------ | --------------------------------------------------------------------- |
| 45020   | 16 / 1           | W only | 2 = load snapshot; 42330 / 0xA55A = APPLY                             |
| 45021   | 16 / 1           | R      | Last APPLY result: 0 success or initial state; nonzero exception code |
| 45022   | 16 / 1           | R      | Stored gateway/native Modbus exception, not low-level LabJack error   |
| 45023   | 16 / 1           | R      | Snapshot loaded flag: 0 no successful load, 1 snapshot loaded         |
| 45024   | 16 / 1           | R      | Count of completed USB write transactions in last APPLY               |
| 45025   | 16 / 1           | R      | Custom map version, 2                                                 |

Unassigned addresses `45013-45019` and `45026-45031` return exception 02. A read of `45020` also returns 02. Do not read across holes or across the write-only command. Safe diagnostic read: address 45021, count 5.

### Load command

```text
FC06 45020 = 2
FC03 45022 count 2 -> expected [0, 1]
```

The loader reads all nine groups of official destinations in the staging table, including both timer configuration pairs even when disabled. Snapshot commit happens only after every required read succeeds. A load failure returns the relevant exception and must not be treated as a successful refresh. If a previous snapshot existed, the loaded flag alone is not proof that the latest load succeeded.

### APPLY command and sequence

```text
FC06 45020 = 42330
```

After validation, the supplied v2 backend performs:

1. Write zero to native number-timers register 50501.
2. Write zero to native counter-mask register 50502.
3. Write FIO mask, EIO mask and pin offset.
4. Write native clock base and divisor.
5. Write Timer0 configuration if requested timer count is at least 1.
6. Write Timer1 configuration if requested timer count is 2.
7. Write requested timer count and counter mask.

The write count includes successful disable transactions. No timer-counter reset command is defined in the custom command register. Native read/reset registers retain firmware semantics.

Validation checks include a loaded snapshot, 8-bit analog masks, pin offset no greater than 8, timer count at most 2, counter mask at most 3, and allocation staying within FIO without analog conflicts. These do not replace every firmware constraint.

APPLY is not atomic. On failure, inspect 45021/45022/45024 and read hardware state. Do not assume the previous configuration was restored. Clock fields are UInt32 native fields; do not apply old easy-API enum assumptions.

### Startup and persistence

Stage, diagnostics and loaded flag start at zero after process initialization; map version remains 2. The gateway does not deliberately reconfigure hardware at startup. Hardware readback can therefore differ from initially zero staging. Process-memory staging is lost on restart and hardware persistence is governed separately by the device.

## Observed snapshot example (not recommended universal defaults)

The successful user-reported load produced:

| Setting       | Hardware value | Staged words          |
| ------------- | -------------- | --------------------- |
| FIO mask      | 255            | 45000 = 255           |
| EIO mask      | 255            | 45001 = 255           |
| Offset        | 4              | 45002 = 4             |
| Timer count   | 0              | 45003 = 0             |
| Counter mask  | 0              | 45004 = 0             |
| Clock base    | 2              | 45005 = 0, 45006 = 2  |
| Divisor       | 0              | 45007 = 0, 45008 = 0  |
| Timer0 config | raw 0x000A0000 | 45009 = 10, 45010 = 0 |
| Timer1 config | raw 0x000A0000 | 45011 = 10, 45012 = 0 |

Both timers were disabled. Raw configuration bits do not imply a timer is running. No clock frequency or timer-mode interpretation is asserted from this snapshot alone.

## Legacy migration

| Legacy custom meaning               | v2 replacement                                                    |
| ----------------------------------- | ----------------------------------------------------------------- |
| 6200-6215 individual analog flags   | Native masks 50590, 50591; decode per-pin bits                    |
| 7100-7115 staging                   | 45000-45012 with the new field layout                             |
| 7199 APPLY                          | 45020 = 42330                                                     |
| 7199 load/reset assumptions         | 45020=2 loads only; use native reset operations where appropriate |
| 7400 APPLY status                   | 45021                                                             |
| 7401 error                          | 45022 exception; 55000 native detailed error                      |
| DIO direction shadow                | Native directions 6100-6119                                       |
| Float32 timer initialization fields | Raw native UInt32 timer config                                    |

No legacy alias is provided. In particular, **7100-7103 now mean native timer configuration**. Old client scripts can modify the wrong function if not migrated.

## Support legend and authority

The official UD table uses x = implemented/tested by LabJack; n = implemented but not fully tested; f = accepted without actual implementation. Blank support cells indicate unsupported operations. The gateway preserves device exceptions rather than upgrading these claims.

Authoritative source: https://support.labjack.com/docs/ud-modbus-old-deprecated
