# Cell communication for GEMS

Software for Maduino Zero 4G LTE cellular board to provide a serial
interface for sending and receiving telemetry data.
Code and interface are on the Arduino Zero compatible MCU,
and communicate with the SIM7600 GSM module via serial.

## Installing

Uses platformio for compile and upload. Main code is in `src/main.cpp`.

## Configuration

* Debug serial and power are on SerialUSB, through usb C port.
* MCU communications interface is on Serial2, using D10 and D11 as TX and RX.

## Interface

Commands and replies begin with `<` and are terminated by `>`. 
All commands return `0` if cell module is not connected.

* `<^>`: check connection. return `1` if connected, `0` if not.
* `<?>`: Check for commands. Return `0` if no reply, `C0` if no command
or other error, `C1` if command is start, `C2` if command is stop.
* `<$>`: Get network time. Returns `0` if error, or `T` followed 
by a 32-bit unix timestamp.
* `<*>`: Get GPS data. Returns `0` if error, `G` followed by comma separated
decimal latitude and longidude if successful.
* `<arbitrary text>` Send data to be logged on server. Typically `[#] x,y,z` 
where `#` is a line number, and the rest is csv data. 
Returns `D0` if error, `Da` if successful.