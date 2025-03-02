# Cell communication for GEMS

Software for Maduino Zero 4G LTE cellular board to provide a serial
interface for sending and receiving telemetry data.
Code and interface are on the Arduino Zero compatible MCU,
and communicate with the SIM7600 GSM module via serial.

## Interface

Commands begin with `<` and are terminated by `>`. 
All commands return 0 if cell module is not connected.

* `<^>`: check connection. return 1 if connected, 0 if not.
* `<?>`: Check for commands. Return 0 if no command or other error, 1 if command is start, 2 if command is stop.
* `<arbitrary text>` Send data to be logged on server. Typically `[#] x,y,z` where `#` is a line number, and the rest is csv data.
  
## Installing

Uses platformio for compile and upload. Main code is in `src/main.cpp`.
