###  MCP42x1 digital potentiometer driver.

The MCP42x1 is an SPI bus operated Dual 7/8-bit digital potentiometer with non-volatile memory.

                        +-----------( )-----------+
                        |  Fn  | pin | pin |  Fn  |
                        |------+-----+-----+------|
                        | CS   |  01 | 14  |  VDD | 1.8V -> 5.5V
                        | SCK  |  02 | 13  |  SDO |
                        | SDI  |  03 | 12  | SHDN |
                        | VSS  |  04 | 11  |   NC |
              ,----- // | P1B  |  05 | 10  |  P0B | // -----,
           R [ ]<--- // | P1W  |  06 | 09  |  P0W | // --->[ ] R
              '----- // | P1A  |  07 | 08  |  P0A | // -----'
                        +-------------------------+

**R** = 5, 10, 50 or 100 kOhms. Wiper resistance = 75 Ohms.

    +---------------------------------------+
    | Fn   | Description                    |
    |------+--------------------------------|
    | CS   | SPI chip select.               |
    | SCK  | SPI clock input.               |
    | SDI  | SPI serial data in.            |
    | VSS  | GND.                           |
    | PxA  | Potentiometer pin A.           |
    | PxW  | Potentiometer wiper.           |
    | PxB  | Potentiometer pin B.           |
    | NC   | Not internally connected.      |
    | SHDN | Hardware shutdown (reset).     |
    | SDO  | Serial data out.               |
    | VDD  | Supply voltage (1.8V to 5.5V). |
    +---------------------------------------+

**NC** is not internally connected but should be externally connected to either VSS or VDD to reduce noise coupling.

Device memory map:

                        +--------------------------------+
                        | Addr | Function          | Mem |
                        |------+-------------------+-----|
                        | 00h  | Volatile wiper 0. | RAM |
                        | 01h  | Volatile wiper 1. | RAM |
                        | 02h  | Reserved.         | --- |
                        | 03h  | Reserved.         | --- |
                        | 04h  | Volatile TCON.    | RAM |
                        | 05h  | Status.           | RAM |
                        | 06h+ | Reserved.         | --- |
                        +--------------------------------+

* All 16 locations are 9 bits wide.
* The status register at 05h has 5 status bits, 4 of which are reserved. Bit 1 is the shutdown status; 0 = normal, 1 = Shutdown.
* The **TCON** register (Terminal Control) has 8 control bits, 4 for each wiper:

            +--------------------------------------------------------------+
            | bit8 | bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 |
            |------+------+------+------+------+------+------+------+------|
            |  D8  | R1HW | R1A  | R1W  | R1B  | R0HW | R0A  | R0W  | R0B  |
            +--------------------------------------------------------------+

**RxHW** : Forces potentiometer x into shutdown configuration of the SHDN pin; 0 = normal, 1 = forced.
**RxA**  : Connects/disconnects potentiometer x pin A to/from the resistor network; 0 = connected, 1 = disconnected.
**RxW**  : Connects/disconnects potentiometer x wiper to/from the resistor network; 0 = connected, 1 = disconnected.
**RxB**  : Connects/disconnects potentiometer x pin B to/from the resistor network; 0 = connected, 1 = disconnected.

* The SHDN pin, when active, overrides the state of these bits.
* The maximum SCK (serial clock) frequency is 10MHz.
* The only SPI modes supported are 0,0 and 1,1.
---
Commands:

The MCP42x1 has 4 commands:

                +-----------------------------------------------------+
                | Command    | Size   | addr  |cmd|       data        |
                |------------+--------+-------+---+-------------------|
                | Read data  | 16-bit |x|x|x|x|1|1|x|x|x|x|x|x|x|x|x|x|
                | Write data | 16-bit |x|x|x|x|0|0|x|x|x|x|x|x|x|x|x|x|
                | Increment  |  8-bit |x|x|x|x|0|1|x|x|-|-|-|-|-|-|-|-|
                | Decrement  |  8-bit |x|x|x|x|1|0|x|x|-|-|-|-|-|-|-|-|
                +---------------------------------+-+-+-+-+-+-+-+-+-+-+
                | Min resistance (x-bit) = 0x000  |0|0|0|0|0|0|0|0|0|0|
                | Max resistance (7-bit) = 0x080  |0|0|1|0|0|0|0|0|0|0|
                | Max resistance (8-bit) = 0x100  |0|1|0|0|0|0|0|0|0|0|
                +-----------------------------------------------------+

---
Testing:

For testing, LEDs were connected via a breadboard as follows:

                        +-----------( )-----------+
                        |  Fn  | pin | pin |  Fn  |
                        |------+-----+-----+------|
               CE0 <----| CS   |  01 | 14  |  VDD |---> +5V
             SCKL1 <----| SCK  |  02 | 13  |  SDO |----> MISO
              MOSI <----| SDI  |  03 | 12  | SHDN |
               GND <----| VSS  |  04 | 11  |   NC |----> GND
                        | P1B  |  05 | 10  |  P0B |
     +5V <--------------| P1W  |  06 | 09  |  P0W |--------------> +5V
     GND <--/\/\/--|<|--| P1A  |  07 | 08  |  P0A |--|>|--/\/\/--> GND
             75R   //   +-------------------------+   \\    75R
                   LED                               LED

The LEDs have a forward voltage and current of 1.8V and 20mA respectively so a 160Ohms resistance is ideal (for 5V VDD) for placing in series with it. However, the wiper resistance is 75Ohms so only an 85Ohms resistor is needed. The closest I have is 75Ohms, which seems fine.

                    R = (5 - 1.8) / 20x10-3 = 160 Ohms.

NC is not internally connected but can be externally connected to VDD or VSS to reduce noise coupling.
