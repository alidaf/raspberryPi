### MCP23017.

    The MCP23017 is an I2C bus operated 16-bit I/O port expander:

                        +-----------( )-----------+
                        |  Fn  | pin | pin |  Fn  |
                        |------+-----+-----+------|
                      { | GPB0 |  01 | 28  | GPA7 | }
                      { | GPB1 |  02 | 27  | GPA6 | }
                      { | GPB2 |  03 | 26  | GPA5 | }
          BANKA GPIOs { | GPB3 |  04 | 25  | GPA4 | } BANKB GPIOs
                      { | GPB4 |  05 | 24  | GPA3 | }
                      { | GPB5 |  06 | 23  | GPA2 | }
                      { | GPB6 |  07 | 22  | GPA1 | }
                      { | GPB7 |  08 | 21  | GPA0 | }
                +5V <---|  VDD |  09 | 20  | INTA |---> Interrupt A.
                GND <---|  VSS |  10 | 19  | INTB |---> Interrupt B.
                      x-|   NC |  11 | 18  | RST  |---> Reset when low.
            I2C CLK <---|  SCL |  12 | 17  | A2   |---}
            I2C I/O <---|  SDA |  13 | 16  | A1   |---} Address.
                      x-|   NC |  14 | 15  | A0   |---}
                        +-------------------------+

            The I2C address device is addressed as follows:

                +-----------------------------------------------+
                |  0  |  1  |  0  |  0  |  A2 |  A1 |  A0 | R/W |
                +-----------------------------------------------+
                : <----------- slave address -----------> :     :
                : <--------------- control byte --------------> :

                R/W = 0: write.
                R/W = 1: read.

            Possible addresses are therefore 0x20 to 0x27 and set by wiring
            pins A0 to A2 low (GND) or high (+5V). If wiring high, the pins
            should be connected to +5V via a 10k resistor.

---

    Reading/writing to the MCP23017:

        The MCP23017 has two 8-bit ports (PORTA & PORTB) that can operate in
        8-bit or 16-bit modes. Each port has associated registers but share
        a configuration register IOCON.

    MCP23017 register addresses:

            +----------------------------------------------------------+
            | BANK1 | BANK0 | Register | Description                   |
            |-------+-------+----------+-------------------------------|
            |  0x00 |  0x00 | IODIRA   | IO direction (port A).        |
            |  0x10 |  0x01 | IODIRB   | IO direction (port B).        |
            |  0x01 |  0x02 | IPOLA    | Polarity (port A).            |
            |  0x11 |  0x03 | IPOLB    | Polarity (port B).            |
            |  0x02 |  0x04 | GPINTENA | Interrupt on change (port A). |
            |  0x12 |  0x05 | GPINTENB | Interrupt on change (port B). |
            |  0x03 |  0x06 | DEFVALA  | Default compare (port A).     |
            |  0x13 |  0x07 | DEFVALB  | Default compare (port B).     |
            |  0x04 |  0x08 | INTCONA  | Interrupt control (port A).   |
            |  0x14 |  0x09 | INTCONB  | Interrupt control (port B).   |
            |  0x05 |  0x0a | IOCON    | Configuration.                |
            |  0x15 |  0x0b | IOCON    | Configuration.                |
            |  0x06 |  0x0c | GPPUA    | Pull-up resistors (port A).   |
            |  0x16 |  0x0d | GPPUB    | Pull-up resistors (port B).   |
            |  0x07 |  0x0e | INTFA    | Interrupt flag (port A).      |
            |  0x17 |  0x0f | INTFB    | Interrupt flag (port B).      |
            |  0x08 |  0x10 | INTCAPA  | Interrupt capture (port A).   |
            |  0x18 |  0x11 | INTCAPB  | Interrupt capture (port B).   |
            |  0x09 |  0x12 | GPIOA    | GPIO ports (port A).          |
            |  0x19 |  0x13 | GPIOB    | GPIO ports (port B).          |
            |  0x0a |  0x14 | OLATA    | Output latches (port A).      |
            |  0x1a |  0x15 | OLATB    | Output latches (port B).      |
            +----------------------------------------------------------+

        The IOCON register bits set various configurations including the BANK
        bit (bit 7):

            +-------------------------------------------------------+
            | BIT7 | BIT6 | BIT5 | BIT4 | BIT3 | BIT2 | BIT1 | BIT0 |
            |------+------+------+------+------+------+------+------|
            | BANK |MIRROR|SEQOP |DISSLW| HAEN | ODR  |INTPOL| ---- |
            +-------------------------------------------------------+

                BANK   = 1: PORTS are segregated, i.e. 8-bit mode.
                BANK   = 0: PORTS are paired into 16-bit mode.
                MIRROR = 1: INT pins are connected.
                MIRROR = 0: INT pins operate independently.
                SEQOP  = 1: Sequential operation disabled.
                SEQOP  = 0: Sequential operation enabled.
                DISSLW = 1: Slew rate disabled.
                DISSLW = 0: Slew rate enabled.
                HAEN   = 1: N/A for MCP23017.
                HAEN   = 0: N/A for MCP23017.
                ODR    = 1: INT pin configured as open-drain output.
                ODR    = 0: INT pin configured as active driver output.
                INTPOL = 1: Polarity of INT pin, active = low.
                INTPOL = 0: Polarity of INT pin, active = high.

                Default is 0 for all bits.

            The internal pull-up resistors are 100kOhm.
