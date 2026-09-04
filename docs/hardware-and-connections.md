# Hardware and Connections

This document records the interfaces represented in the consolidated ESP32
firmware. Verify voltage levels, driver stages, common grounding, and the actual
sensor-module pinout before wiring hardware.

## Controller pin map

| Function | GPIO | Interface | Firmware behavior |
| --- | ---: | --- | --- |
| DHT22 data | 4 | Digital | Reads temperature and relative humidity every 2 s |
| Reservoir state | 34 | Digital input-only | Publishes the current state to `/wl` |
| Soil moisture | 35 | ADC1 input-only | Compares the raw reading with the irrigation threshold |
| PIR | 27 | Digital | Contributes to the local alarm condition |
| LDR | 33 | ADC1 | Contributes to the local alarm condition |
| Pump command | 26 | Digital | Drives an external pump-control stage |
| Buzzer | 13 | PWM-capable digital | Generates a 1 kHz alarm tone |
| Status LED | 5 | Digital | Indicates the configured humidity condition |

## Electrical implementation notes

- GPIO 34 and GPIO 35 are input-only ESP32 pins.
- GPIO 33 is on ADC1 and therefore remains available while Wi-Fi is active.
- The pump requires a MOSFET, relay, or motor-driver stage sized for the load.
- Add flyback protection across inductive DC loads when the driver does not
  provide it internally.
- Use an external supply for the pump and connect logic and power grounds at a
  controlled common reference.
- Confirm that every sensor output is compatible with 3.3 V ESP32 inputs.
- The present firmware reports the reservoir state but does not use it as a
  pump interlock. Add and physically validate this interlock before unattended
  operation.

## Calibration requirements

`SOIL_IRRIGATION_THRESHOLD` preserves the threshold and polarity used by the
original project sketches. Record readings in dry soil, correctly watered soil,
and saturated soil before selecting a deployment threshold.

The LDR threshold must likewise be measured with the actual voltage-divider
orientation. Reverse the comparison only when the circuit test confirms that
the measured polarity is opposite to the firmware assumption.

