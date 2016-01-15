###LM27966 LED driver with I2C interface:

The **LM27966** is a charge-pump based display LED driver with an **I2C** compatible interface that can drive up to 6 LEDs in 2 banks. The main bank of 4 or 5 LEDs is intended as a backlight to a display and the secondary bank of 1 LED as a general purpose indicator. The brightness of each bank can be controlled independently via the **I2C** interface.

The **LM27966** is only available in a WQFN package, so not easy to prototype with unless there are breakout boards available.

                +-------------------------+
                |    06 05 04 03 02 01    |
                | 07                   24 |
                | 08                   23 |
                | 09                   22 |
                | 10                   21 |
                | 11                   20 |
                | 12                   19 |
                |    13 14 15 16 17 18    |
                +-------------------------+

####Pin functions:

    +-------------------------------------------+
    | No | Fn    | Description                  |
    |----+-------+------------------------------|
    | 01 | SCL   | Serial clock.                |
    | 02 | SDIO  | Serial data IO.              |
    | 03 | Daux  | LED driver (auxiliary).      |
    | 04 | NC    | Not connected.               |
    | 05 | NC    | Not connected.               |
    | 06 | NC    | Not connected.               |
    | 07 | VIO   | Serial bus voltage.          |
    | 08 | NC    | Not connected.               |
    | 09 | GND   | Ground.                      |
    | 10 | RESET | Hardware reset #1.           |
    | 11 | NC    | Not connected.               |
    | 12 | D5    | LED driver (main display).   |
    | 13 | D4    | LED driver (main display).   |
    | 14 | D3    | LED driver (main display).   |
    | 15 | D2    | LED driver (main display).   |
    | 16 | D1    | LED driver (main display).   |
    | 17 | Iset  | Set full scale current #2.   |
    | 18 | GND   | Ground.                      |
    | 19 | C1    | Flying capacitor connection. |
    | 20 | C2    | Flying capacitor connection. |
    | 21 | C2    | Flying capacitor connection. |
    | 22 | C1    | Flying capacitor connection. |
    | 23 | Pout  | Charge pump output voltage.  |
    | 24 | Vin   | Input voltage 2.7V - 5.5V.   |
    +-------------------------------------------+

 Notes:
 1. High = normal operation, Low = reset.
 2. Placing a resistor (Rset) between this pin and GND sets the full-scale current for Dx and Daux LEDs (LED current = 200 x (1.25V / Rset).

####Registers:

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

