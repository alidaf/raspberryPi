###Popcorn Hour C200 Hardware:

This library is intended to provide some low level controls to allow re-purposing of the **C200** case with a **Raspberry Pi** for use as a media server and Squeezebox client.

The **C200** front panel has a 192x64 pixel display, 2 USB ports and some function buttons. The function of the front panel is provided by the following components:

See http://hardwarebug.org/2015/02/26/popcorn-hour-revisited/

 * **AMG9264C** display.
 * **MAX7325** I2C port expander.
 * **LM27966** backlight controller.

There are two connectors to the front panel that provide access to the display and USBs:

    J1 - Front Panel.                               J8 - Front USBs.
    +-------------------------------------------+   +----------------+
    | Pin | Function    | Description           |   | Pin | Function | 
    |-----+-------------+-----------------------|   |-----+----------|
    |   1 | +5V         | Supply voltage.       |   |  1  | VCC      |
    |   2 | GND         | Ground.               |   |  2  | Data0 -  |
    |   3 | 5VSTB       | ATX standby power.    |   |  3  | Data0 +  |
    |   4 | GND         | GND.                  |   |  4  | GND      |
    |   5 | RC_OUT      | IR out.               |   |  5  | VCC      |
    |   6 | GND         | Ground                |   |  6  | Data1 -  |
    |   7 | RC_IN       | IR in.                |   |  7  | Data1 +  |
    |   8 | RESET       | Reset (when low).     |   |  8  | GND      |
    |   9 | SDA         | I2C data.             |   +----------------+
    |  10 | SCL         | I2C clock.            |
    |  11 | STB_LED     | Standby LED.          |
    |  12 | POW_ON      | ATX power on.         |
    |  13 | POW_ON_LED  | Power on LED.         |
    |  14 | TEST        | Test.                 |
    +-------------------------------------------+

 * Front panel button presses are encoded by a separate microcontroller and sent via the **RC_OUT** pin using NEC encoded codes.
 * The **RC_IN** receives NEC encoded controls and duplicates them on the **RC_OUT** pin.
 * the **STB_LED** pin activates an amber LED and also resets the **MAX7325** and **LM27966**.

---
####AMG19264 display.

Pin layout:

        +-----------------------------------------------------+
        | Pin | Label | Description                           |
        |-----+-------+---------------------------------------|
        |   1 |  Vss  | Ground (0V) for logic.                |
        |   2 |  Vdd  | 5V supply for logic.                  |
        |   3 |  Vo   | Variable V for contrast.              |
        |   4 |  Vee  | Voltage output.                       |
        |   5 |  RS   | Register Select. 0: command, 1: data. |
        |   6 |  RW   | R/W. 0: write, 1: read.               |
        |   7 |  E    | Enable bit.                           |
        |   8 |  DB0  | Data bit 0.                           |
        |   9 |  DB1  | Data bit 1.                           |
        |  10 |  DB2  | Data bit 2.                           |
        |  11 |  DB3  | Data bit 3.                           |
        |  12 |  DB4  | Data bit 4.                           |
        |  13 |  DB5  | Data bit 5.                           |
        |  14 |  DB6  | Data bit 6.                           |
        |  15 |  DB7  | Data bit 7.                           |
        |  16 |  CS3  | Chip select (left).                   |
        |  17 |  CS2  | Chip select (middle).                 |
        |  18 |  CS1  | Chip select (right).                  |
        |  19 |  RST  | Reset signal.                         |
        |  20 |  BLA  | Voltage for backlight (max 5V).       |
        |  21 |  BLK  | Ground (0V) for backlight.            |
        +-----------------------------------------------------+

 * Most displays are combinations of up to 3 64x64 modules, each controlled via the CSx (chip select) registers.

**AMG19264** register bits:

        +-------+ +-------------------------------+
        |RS |RW | |DB7|DB6|DB5|DB4|DB3|DB2|DB1|DB0|
        |---+---| |---+---+---+---+---+---+---+---|
        | 0 | 0 | | 0 | 0 | 1 | 1 | 1 | 1 | 1 | D |
        | 0 | 0 | | 0 | 1 | Y | Y | Y | Y | Y | Y |
        | 0 | 0 | | 1 | 0 | 1 | 1 | 1 | P | P | P |
        | 0 | 0 | | 1 | 1 | X | X | X | X | X | X |
        | 0 | 1 | | B | 1 | S | R | 0 | 0 | 0 | 0 |
        | 1 | 0 | |   :   : Write Data:   :   :   |
        | 1 | 1 | |   :   : Read Data :   :   :   |
        +-------+ +-------------------------------+

Key:

       +-------------------------+
       | Key | Effect            |
       +-----+-------------------+
       |  D  | Display on/off.   |
       |  B  | Busy status.      |
       |  S  | Display status.   |
       |  R  | Reset status.     |
       |  X  | X address (0-63). |
       |  Y  | Y address (0-63). |
       |  P  | Page (0-7).       |
       +-------------------------+

Some **AMG19264** commands:

    +-------+ +---------------------+
    |RS |RW | | Cmd  | Function.    |
    |---+---| |------+--------------|
    | 0 | 0 | | 0x3e | Display off. |
    | 0 | 0 | | 0x3f | Display on.  |
    | 0 | 0 | | 0x40 | Y position.  |
    | 0 | 0 | | 0xc0 | X position.  |
    | 0 | 0 | | 0xb8 | Page.        |
    | 0 | 1 | | 0x40 | Read status. |
    +-------+ +---------------------+

---
####MAX7325 I2C port expander:

The **MAX7325** is an **I2C** interface port expander that features 16 I/O ports, divided into 8 push-pull outputs and 8 I/Os with selectable internal pull-ups and transition detection.
The **MAX7325** occupies 2 **I2C** addresses, which are set according to the pin connections AD2 and AD0. These pins can be connected to either V+, GND, SDA or SCL, the combination of which gives 16 possible addresses and also sets the power up port states.
The addresses used in the **C200** are 0x5d and 0x6d. From Tables 2 and 3 of the reference, these translate as:

    +-----------------------------------------------------------------------+
    | addr |O15|O14|O13|O12|O11|O10| O9| O8|| P7| P6| P5| P4| P3| P2| P1| P0|
    |------+---+---+---+---+---+---+---+---++---+---+---+---+---+---+---+---|
    | 0x5d | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 || - | - | - | - | - | - | - | - | 
    | 0x6d | - | - | - | - | - | - | - | - || 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 
    +-----------------------------------------------------------------------+

#####Addressing:

The 8th bit of the slave address is the R/W bit, low for write, high for read.

Reading:
 * A single-byte read from the I/O ports (P0-P7) returns the status of the ports and clears the transition flags and interrupt output.
 * A two-byte read from the I/O ports (P0-P7) returns the status of the ports followed by the transition flags as the 2nd byte.
 * A multi-byte read from the I/O ports repeatedly returns the port status followed by the transition flags.
 * Single-byte and multi-byte reads from the output ports (O8-O15) return the status either singly or continuously. 
Writing:
 * A single byte write to either port group sets the logic state of all 8 ports.
 * A multibyte write repeatedly sets the logic state of all 8 ports.

####LM27966 LED driver.

The **LM27966** is a charge-pump based display LED driver with an **I2C** compatible interface that can drive up to 6 LEDs in 2 banks. The main bank of 4 or 5 LEDs is intended as a backlight to a display and the secondary bank of 1 LED as a general purpose indicator. The brightness of each bank can be controlled independently via the **I2C** interface.

 * The **I2C** address is fixed at 0x36.
 * There are three registers:

        +--------------------------------------------------+
        | Register                                  | Addr |
        |-------------------------------------------+------|
        | General purpose register                  | 0x10 |
        | Main display brightness control register  | 0xa0 |
        | Auxiliary LED brightness control register | 0xc0 |
        +--------------------------------------------------+

#####General purpose register:

    +---------------------------------------------------------------+
    | bit7  | bit6  | bit5  | bit4  | bit3  | bit2  | bit1  | bit0  |
    |-------+-------+-------+-------+-------+-------+-------+-------|
    |   0   |   0   |   1   |  T1   | EN-D5 |EN-AUX |  T0   |EN_MAIN|
    +---------------------------------------------------------------+

    +----------------------------------------+
    | Fn      | Description                  |
    |---------+------------------------------|
    | EN-MAIN | Enable Main LED drivers.     |
    | T0      | Must be set to 0.            |
    | EN-AUX  | Enable Daux LED driver.      |
    | EN-D5   | Enable D5 LED voltage sense. |
    | T1      | Must be set to 0.            |
    +----------------------------------------+

#####Main display brightness control register:

    +-------------------------------------------------------+
    | bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 |
    |------+------+------+------+------+------+------+------|
    |   1  |   1  |   1  |  Dx4 |  Dx3 |  Dx2 |  Dx1 |  Dx0 |
    +-------------------------------------------------------+

 * Dx4-Dx0 sets brightness for Dx LEDs (full-scale = 11111).
 * Perceived brightness is from 0x00 (1.25%) to 0x1f (100%).

#####Auxiliary LED brightness control register:

    +-------------------------------------------------------+
    | bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 |
    |------+------+------+------+------+------+------+------|
    |   1  |   1  |   1  |   1  |   1  |   1  |Daux1 |Daux0 |
    +-------------------------------------------------------+

 * Daux1-Daux2 sets brightness of auxliliary LED (full-scale = 11).
 * Perceived brightness is from 0x0 (20%) to 0x3 (100%).

####Coding:

The LED brightness is set by sending 3 bytes to the **I2C** bus in sequence, MSB first:

 1. Chip address (0x36).
 2. Register address.
 3. Data.

####NEC control codes.

see https://techdocs.altium.com/display/FPGA/NEC+Infrared+Transmission+Protocol.

The NEC transmission protocol is a 38kHz carrier with pulse distance encoding of the message bits. The logical bits are transmitted as follows:

 * 0 (low)  : 562.5uS pulse burst followed by a 562.5uS space.
 * 1 (high) : 562.5uS pulse burst followed by a 1.6875uS space.

For remote controls, a key press is transmitted with LSB first as follows:

 * 9ms pulse (start of transmission).
 * 4.5ms space.
 * 8-bit address for the receiving device.
 * 8-bit logical inverse of the address.
 * 8-bit command.
 * 8-bit logical inverse of the command.
 * 562.5us pulse (end of transmission).

Repeat code (keeping a key pressed) is:

 * 9ms pulse (start of transmission).
 * 2.25ms space.
 * 562.5us pulse (end of transmission).
