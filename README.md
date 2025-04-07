# Fan Monitor

A Linux CLI and daemon for monitoring system temperature in real time, storing it in shared memory, and providing flexible CLI control. It is dedicated to use on a Raspberry Pi board, and tuned according to use with a Noctua fan.
For testing, it was used on a Raspberry Pi 4 Model B along with a NF-A4x10 5V PWM fan, and making a good use of the of the pulse-width modulation pin.

## Features

- Run as a background daemon
- Shared memory interface
- Real-time temperature updates
- Configurable struct-based storage
- CLI commands for get/set/start/stop

## Requirements

- Linux OS
- GCC
- sudo privileges for starting daemon

## Build

```bash
make
```
or

```bash
gcc temp_monitor_daemon.c -o temp_monitor_daemon -lrt -lpi-gpio -lcjson  && gcc setFan.c -o setFan -lrt -lpi-gpio -lcjson
```

## Usage

### Set a static value (between 0% and 100% fan speed)
```bash
setFan <int>
```
As an example:
```bash
# set fan speed to 75%
setfan 75
```
Observation: setting a static value won't need to start the daemon

### Start the daemon (and use current configuration)
```bash
setFan start
```
Observation: configuration is found in `config.json`, modify as you wish

### Stop the daemon
```bash
setFan stop
```

## Roadmap

- [ ] Support for saving updated config back to config.json

- [ ] Add unit tests and test suite

- [ ] Logging system for daemon actions and temperature trends

- [ ] Add verbose/debug mode in CLI

- [ ] Validate config fields on load

- [ ] Add status CLI command for daemon health and config snapshot

- [ ] Cross-platform support (basic POSIX-compatible systems)

- [ ] Optional Web UI or HTTP interface (experimental)

- [ ] Daemon auto-restart with systemd integration

- [ ] Code refactoring for ease of reading