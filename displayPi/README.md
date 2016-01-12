### HD44780

    Pin layout for Hitachi HD44780 based LCD display.

        +------------------------------------------------------------+
        | Pin | Label | Pi   | Description                           |
        |-----+-------+------+---------------------------------------|
        |   1 |  Vss  | GND  | Ground (0V) for logic.                |
        |   2 |  Vdd  | 5V   | 5V supply for logic.                  |
        |   3 |  Vo   | xV   | Variable V for contrast.              |
        |   4 |  RS   | GPIO | Register Select. 0: command, 1: data. |
        |   5 |  RW   | GND  | R/W. 0: write, 1: read. *Caution*     |
        |   6 |  E    | GPIO | Enable bit.                           |
        |   7 |  DB0  | n/a  | Data bit 0. Not used in 4-bit mode.   |
        |   8 |  DB1  | n/a  | Data bit 1. Not used in 4-bit mode.   |
        |   9 |  DB2  | n/a  | Data bit 2. Not used in 4-bit mode.   |
        |  10 |  DB3  | n/a  | Data bit 3. Not used in 4-bit mode.   |
        |  11 |  DB4  | GPIO | Data bit 4.                           |
        |  12 |  DB5  | GPIO | Data bit 5.                           |
        |  13 |  DB6  | GPIO | Data bit 6.                           |
        |  14 |  DB7  | GPIO | Data bit 7.                           |
        |  15 |  A    | xV   | Voltage for backlight (max 5V).       |
        |  16 |  K    | GND  | Ground (0V) for backlight.            |
        +------------------------------------------------------------+

    LCD register bits:

        +---------------------------------------+   +-------------------+
        |RS |RW |DB7|DB6|DB5|DB4|DB3|DB2|DB1|DB0|   |Key|Effect         |
        |---+---+---+---+---+---+---+---+---+---|   |---+---------------|
        | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 |   |I/D|DDRAM inc/dec. |
        | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | - |   |R/L|Shift R/L.     |
        | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 |I/D| S |   |S  |Shift on.      |
        | 0 | 0 | 0 | 0 | 0 | 0 | 1 | D | C | B |   |DL |4-bit/8-bit.   |
        | 0 | 0 | 0 | 0 | 0 | 1 |S/C|R/L| - | - |   |D  |Display on/off.|
        | 0 | 0 | 0 | 0 | 1 |DL | N | F | - | - |   |N  |1/2 lines.     |
        | 0 | 0 | 0 | 1 |   : CGRAM address :   |   |C  |Cursor on/off. |
        | 0 | 0 | 1 |   :   DDRAM address   :   |   |F  |5x8/5x10 font. |
        | 0 | 1 |BF |   :   Address counter :   |   |B  |Blink on/off.  |
        | 1 | 0 |   :   : Read Data :   :   :   |   |S/C|Display/cursor.|
        | 1 | 1 |   :   : Write Data:   :   :   |   |BF |Busy flag.     |
        +---------------------------------------+   +-------------------+

    DDRAM: Display Data RAM.
    CGRAM: Character Generator RAM.
