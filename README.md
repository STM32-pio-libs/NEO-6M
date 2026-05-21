# STM32 NEO-6M

TinyGPS-style NMEA parser and interrupt-driven UART receive helper for the u-blox **NEO-6M** GPS module on STM32.

## Features

- byte-by-byte `NEO6M_Encode()` parser
- decoded data stored in a single handle
- checksum-verified NMEA sentence parsing
- parsed `RMC` and `GGA` sentence support
- decoded location, date, time, speed, course, satellites, HDOP, altitude, and fix quality
- optional UART RX interrupt adapter with no `main.h` dependency in the public header

## Parsed Data

The driver updates these values when valid NMEA data is received:

- latitude and longitude in degrees scaled by `1e7`
- UTC date and time
- speed in knots scaled by `100`
- course in degrees scaled by `100`
- satellites count
- HDOP scaled by `100`
- altitude in centimeters
- fix quality and fix state

Each field carries `valid` and `updated` flags similar to TinyGPS-style usage.

## Quick Start

1. Initialize the handle with `NEO6M_Init(&gps)`.
2. Feed incoming bytes using `NEO6M_Encode(&gps, byte)`.
3. Read decoded fields from the handle after `updated` flags are set.

### Minimal Parser Flow

```c
NEO6M_HandleTypeDef gps;

NEO6M_Init(&gps);

while (have_next_byte()) {
    uint8_t ch = next_byte();
    (void)NEO6M_Encode(&gps, (char)ch);

    if (gps.location.updated != 0U) {
        int32_t lat = gps.location.latitude_deg_e7;
        int32_t lon = gps.location.longitude_deg_e7;
        NEO6M_ClearUpdates(&gps);
    }
}
```

## Interrupt-Driven UART RX

For STM32 HAL projects, the library can own the one-byte receive buffer and re-arm reception automatically:

1. Attach a UART receive callback with `NEO6M_AttachUart()`
2. Start reception with `NEO6M_StartReceiveIT()`
3. Call `NEO6M_RxCpltCallback()` from `HAL_UART_RxCpltCallback()`

See `examples/usart2_rx_it_hal.c`.

## USART2 Note

If you use `USART2` for GPS RX:

- configure the UART for `9600 8N1`
- only RX is required from the STM32 side
- on many STM32 MCUs, `USART2` RX is on `PA3`
- verify your exact TX or alternate pin mapping in the datasheet before assuming `PA4`

## Return Codes

- `NEO6M_OK`: success
- `NEO6M_ERR_INVALID_ARG`: invalid argument or missing callback
- `NEO6M_ERR_IO`: transport callback failure
