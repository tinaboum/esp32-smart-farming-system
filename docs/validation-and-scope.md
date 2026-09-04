# Validation and Scope

## Evidence available

- Original project report describing the greenhouse demonstrator.
- Two historical ESP32/Arduino sketches.
- Firebase Realtime Database paths used by the embedded code.
- Android monitoring-interface screenshots.
- Portfolio documentation derived from those sources.

## Implemented and evidenced

- ESP32-based sensor acquisition.
- DHT22 temperature and air-humidity monitoring.
- Analog soil-moisture acquisition.
- Reservoir-level/state input.
- Rule-based, timed DC-pump control.
- PIR/LDR buzzer condition and humidity status output.
- Wi-Fi and Firebase telemetry.
- Android presentation of greenhouse measurements.
- Physical prototype-level demonstration documented in the project material.

## Not evidenced as part of the implemented firmware

- Gas or smoke sensing.
- Flame detection.
- Remote actuator commands from the Android application.
- Closed-loop fan control.
- Servo-valve control.
- Calibrated soil-moisture percentage conversion.
- AI-based control or prediction.
- Quantified water savings, crop-yield improvement, control accuracy, or
  long-duration reliability.

## Status of the repository firmware

The repository sketch consolidates and cleans the two historical source files.
It preserves the original four Firebase paths and the documented control
behavior while replacing credential values with placeholders and moving two
signals away from problematic ESP32 pins.

The consolidated code has been syntax-checked in a controlled environment. It
must still be compiled with the selected ESP32/Firebase library versions and
revalidated on the actual circuit before hardware deployment.

