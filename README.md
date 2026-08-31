# LabJack U3 Modbus TCP Server (C, Linux/Raspberry Pi)

This project implements a **Modbus TCP server** in C that exposes all I/O of a **LabJack U3-LV** device (analog inputs, digital I/O, DAC outputs, timers, counters) over Modbus. It runs on Linux (e.g., Raspberry Pi) and uses the official LabJack **Exodriver** (`liblabjackusb`) for USB communication.

No Python is used; everything is pure C.

---

## Features

- **Full U3-LV I/O mapping:**
  - Digital I/O (FIO0–FIO7, EIO0–EIO7, CIO0–CIO3)
  - Analog inputs (AIN0–AIN15) with per-channel scaling and offset
  - Analog outputs (DAC0, DAC1)
  - Timers (Timer0, Timer1) with configurable mode and value
  - Counters (Counter0, Counter1) with scaling, offset, and remote reset
- **Modbus TCP server** on port 5020 (configurable in `main.c`).
- **Scaling & calibration:**
  - Per-channel AIN scale/offset
  - Global counter scale/offset
- **Diagnostics:**
  - Last error code register
  - Uptime counter
  - Placeholder for firmware version
- **System integration:**
  - `Makefile` with `build`, `install`, `systemd`, and `uninstall` targets.
  - Optional systemd service for auto-start on boot.

---

## Hardware & Software Requirements

- **Hardware:**
  - LabJack U3-LV device
  - Raspberry Pi or other Linux machine with USB host
- **Software:**
  - Linux (Debian/Raspberry Pi OS recommended)
  - GCC, make, libusb-1.0 development files
  - LabJack Exodriver sources (this project assumes the Exodriver tree at:
    `/home/benjiworld/lj/exodriver-master`)

---

## Directory Layout (Typical)

Assume a structure like:

```text
/home/benjiworld/lj/prova/
  main.c
  modbus_u3.h
  modbus_u3.c
  u3_easy_wrap.c
  Makefile
  README.md

/home/benjiworld/lj/exodriver-master/
  examples/U3/
    u3.h
    u3.c
    ...
  liblabjackusb/
    labjackusb.h
    liblabjackusb.so (or .a)
    ...
```

Adjust paths in the `Makefile` if your Exodriver is located elsewhere.

---

## Modbus Map

All addresses are zero-based as used in Modbus TCP.

### Coils (Read/Write)

| Address | Description                    |
|---------|--------------------------------|
| 0–19    | DIO0–DIO19 (FIO/EIO/CIO)       |
| 20      | Timer0 enable (0/1)            |
| 21      | Timer1 enable (0/1)            |
| 22      | Counter0 enable (0/1)          |
| 23      | Counter1 enable (0/1)          |

### Discrete Inputs (Read-only)

| Address | Description                    |
|---------|--------------------------------|
| 0–19    | DIO0–DIO19 state (same as coils 0–19) |

### Holding Registers (Read/Write)

| Address | Description                                      |
|---------|--------------------------------------------------|
| 0–15    | AIN0–AIN15 (0–10000 engineering units)           |
| 100     | DAC0 setpoint (0–10000 → 0–5 V)                  |
| 101     | DAC1 setpoint (0–10000 → 0–5 V)                  |
| 200     | Timer0 mode (see timer mode enums)               |
| 201     | Timer0 value (0–65535)                           |
| 202     | Timer1 mode                                      |
| 203     | Timer1 value (0–65535)                           |
| 210     | Reset counters (write-only): bit0=C0, bit1=C1    |
| 211     | Timer clock base index                           |
| 212     | Timer clock divisor                              |
| 300–315 | AIN0–AIN15 scale (default 10000 = 1.0000)        |
| 316–331 | AIN0–AIN15 offset (default 0)                    |
| 340     | Counter scale (default 10000 = 1.0000)           |
| 341     | Counter offset (default 0)                       |
| 400     | Last error code (diagnostic)                     |
| 401     | Uptime in seconds (0–65535, diagnostic)          |

### Input Registers (Read-only)

| Address | Description                                      |
|---------|--------------------------------------------------|
| 0       | DAC0 actual (0–10000, last setpoint)             |
| 1       | DAC1 actual (0–10000, last setpoint)             |
| 2       | Counter0 value (0–65535, scaled)                 |
| 3       | Counter1 value (0–65535, scaled)                 |
| 10      | Firmware version (placeholder, currently 0)      |

---

## Scaling Details

### Analog Inputs (AIN)

Each AIN channel has:

- **Scale** (Holding 300–315): default 10000 = 1.0000  
- **Offset** (Holding 316–331): default 0  

Raw voltage (0–~2.44 V for U3-LV) is converted internally, then:

\[
\text{EngineeringUnits} = \text{Volts} \times \frac{\text{scale}}{10000} + \frac{\text{offset}}{10000}
\]

The Holding register value is:

\[
\text{Register} = \text{EngineeringUnits} \times \frac{10000}{10}
\]

clamped to 0–10000.

Example:  
- AIN0 scale = 10000 (1.0), offset = 0 → 2.44 V → ~10000 register value.  
- AIN0 scale = 20000 (2.0), offset = 1000 (+0.1) → 1.0 V →  
  Eng = 1.0 × 2.0 + 0.1 = 2.1 → register ≈ 2100.

### DAC Outputs

Holding registers 100/101:

- 0 → 0 V  
- 10000 → 5 V  
- Linear mapping: `Volts = value * (5.0 / 10000.0)`

Input registers 0/1 return the last setpoint in the same 0–10000 scale.

### Counters

Counters are read from hardware (0–65535 typical range), then scaled:

\[
\text{Scaled} = \text{Raw} \times \frac{\text{counterScale}}{10000} + \frac{\text{counterOffset}}{10000}
\]

clamped to 0–65535.

Example:  
- counterScale = 10000 (1.0), offset = 0 → raw 1234 → 1234.  
- counterScale = 5000 (0.5), offset = 100 (+0.01) → raw 1000 → 500 + 0.01 ≈ 500.

---

## Building

Ensure the Exodriver is available at the expected path, then:

```bash
make
```

This produces `modbus_u3_server` in the current directory.

To clean and rebuild:

```bash
make clean
make
```

---

## Installation

### Install binary only

```bash
sudo make install
```

Copies `modbus_u3_server` to `/usr/local/bin/modbus_u3_server`.

### Install as systemd service

```bash
sudo make systemd
```

This:

- Installs the binary (if not already installed).
- Creates `/etc/systemd/system/modbus-u3.service`.
- Reloads systemd.

Then enable and start:

```bash
sudo systemctl enable modbus-u3
sudo systemctl start modbus-u3
```

Check status:

```bash
sudo systemctl status modbus-u3
```

Logs:

```bash
journalctl -u modbus-u3 -f
```

### Uninstall

```bash
sudo systemctl stop modbus-u3
sudo systemctl disable modbus-u3
sudo make uninstall
```

---

## Running Manually

If not using systemd:

```bash
sudo ./modbus_u3_server
```

The server listens on TCP port **5020** by default (edit `PORT` in `main.c` to change).

---

## Example Modbus Operations

Using any Modbus TCP client (PLC, SCADA, `modpoll`, etc.):

### Read AIN0

- Function: 03 (Read Holding Registers)  
- Address: 0  
- Count: 1  
- Result: 0–10000 representing AIN0 voltage with scaling.

### Write DAC0 to 2.5 V

- Desired value: 2.5 V → 2.5 / 5.0 × 10000 = 5000  
- Function: 06 (Write Single Register)  
- Address: 100  
- Value: 5000  

### Read Counter0

- Function: 04 (Read Input Registers)  
- Address: 2  
- Count: 1  

### Reset Counter0

- Function: 06  
- Address: 210  
- Value: 1 (bit0 = reset C0)

### Change AIN0 Scale to 2.0

- Scale 2.0 → 20000  
- Function: 06  
- Address: 300  
- Value: 20000

---

## Timer Modes

Timer mode values (Holding 200, 202) use LabJack’s standard enums, e.g.:

- 0 = 16-bit PWM  
- 1 = 8-bit PWM  
- 2 = Period input (32-bit, rising)  
- 4 = Duty cycle input  
- 7 = Frequency output  
- etc. (see `u3.h` / LabJack documentation for full list).

Timer values (Holding 201, 203) are 0–65535 and interpreted according to the mode.

---

## Diagnostics

- **Holding 400**: Last error code from internal calls (0 = no error).  
  - Write 0 to clear.
- **Holding 401**: Uptime in seconds since server start (0–65535).  
- **Input 10**: Firmware version (currently placeholder 0; can be extended).

---

## Customization

- **Port number**: Edit `#define PORT` in `main.c`.
- **Default timer/counter config**: Adjust initial values in `open_u3()` in `modbus_u3.c`.
- **Scaling defaults**: Change default `ainScale`, `ainOffset`, `counterScale`, `counterOffset` in `open_u3()`.
- **Additional registers**: Extend `mb_read_holding_register`, `mb_write_holding_register`, and `mb_read_input_register` as needed.

---

## Troubleshooting

- **Device not found:**  
  - Ensure the U3 is connected via USB.  
  - Check `dmesg` or `lsusb` for the device.  
  - Verify Exodriver and `liblabjackusb` are correctly installed.

- **Modbus client cannot connect:**  
  - Ensure port 5020 is not blocked by a firewall.  
  - Confirm the server is running (`sudo systemctl status modbus-u3` or `ps aux | grep modbus_u3_server`).

- **Unexpected register values:**  
  - Verify scaling/offset registers.  
  - Check last error code (Holding 400).  
  - Inspect server logs (`journalctl -u modbus-u3 -f`).

---

## License & Credits

This project uses the LabJack Exodriver (“easy” functions and low-level USB API) from LabJack Corporation. See LabJack’s licensing terms for the Exodriver.

The Modbus server implementation and mapping logic in this repository are provided as-is for integration with LabJack U3 devices.

---

## Contact / Support

- For U3 hardware and Exodriver details: see [LabJack documentation](https://labjack.com).
- For this Modbus server implementation: adapt the code as needed for your application; the register map and scaling can be customized by editing `modbus_u3.c` and `modbus_u3.h`.
