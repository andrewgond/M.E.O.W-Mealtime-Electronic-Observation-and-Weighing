M.E.O.W. Power Block – README
Overview

The Power Block is the entry point of the M.E.O.W. system’s electrical architecture. It receives standard U.S. wall power and converts it into a regulated DC bus consisting of 12 V, 5 V, and 3.3 V rails. Every subsystem—microcontroller, sensors, UI, enclosure/kinematics—depends on these rails for reliable operation.
This block ensures:

Safe AC input handling

Stable and efficient DC conversion

Overcurrent tolerance

Noise filtering

Reliable power distribution to all downstream subsystems

Description

The block accepts 120 VAC, 60 Hz from a standard U.S. wall outlet (AC_pwr_in). A 12 V / 2 A barrel-jack power adapter performs the primary AC→DC conversion. Inside the system:

12 V Rail
Used for higher-voltage components such as stepper motors.

Buck Converters (LM2596S)

12 V → 5 V

12 V → 3.3 V

These rails power logic-level components such as microcontrollers, stepper drivers, and sensors.
This modular barrel-jack architecture improves maintainability and allows easy cable replacement.

Design
AC Input

A standard NEMA wall connector feeds into a barrel jack mounted on the PCB. A Schottky diode (1N5820) provides reverse-polarity protection.

DC Conversion

Two LM2596S buck converters were chosen due to:

Good documentation and widespread use

Easy sourcing

Reliable regulation characteristics

Each module uses input/output capacitors sized according to TI guidelines and Digi-Key’s evaluation module documentation.

PCB Layout

Designed in KiCad

Through-hole components used to support open-source and ease of assembly

1.5 mm traces for high-current paths

Screw terminals used for output rails during testing

Total PCB size approximately that of a credit card

Design Artifacts

Figure 3.1.1 – KiCad Schematic

Figure 3.1.2 – KiCad PCB Trace Layout

Figure 3.1.3 – KiCad PCB 3D Model

General Validation

The power block performs:

AC→DC conversion to a regulated 12 V rail

12 V→5 V via buck converter

12 V→3.3 V via secondary buck converter

All values conform to the top-level system plan and meet requirements for enclosure/kinematics, sensors, microcontroller, UI, and code/database subsystems.

Interface Validation

Below are the validated interfaces using the naming convention from_to_type.

Interface 1: ext_power_acpwr
Property	Value	Rationale	Verification
V_rms	120 VAC @ 60 Hz	U.S. mains spec	AC–DC adapter supports 90–132 VAC @ 47–63 Hz
I_in_max	≤ 0.5 A	15 W at 120 V draws ≈0.17 A, so 0.5 A margin is safe	Adapter spec sheet
Connector	2-pin shielded AC inlet + barrel connector	Matches AC adapter form factor	Verified by physical assembly
Interface 2: power_enc_dcpwr12v
Property	Value	Rationale	Verification
V_min	11.1 V	12 V ±7.5% tolerance	Measured via multimeter no-load
V_max	12.9 V	Upper rail tolerance	Measured under load
I_nom	250 mA	Expected stepper motion draw	Below motor driver limits
I_peak	300 mA	Max expected transient load	Verified with electronic load testing
Interface 3: dc_pwr_out_5v
Property	Value	Rationale	Verification
V_min	4.625 V	5 V −7.5% tolerance	LM2596S datasheet limits
V_max	5.375 V	5 V +7.5% tolerance	Open-circuit tests
I_nom	100 mA	Sensors + microcontroller	ESP32 typical current 5 mA
I_peak	300 mA	Surges / peripherals	Verified with electronic load
Interface 4: dc_pwr_out_3.3v
Property	Value	Rationale	Verification
V_min	3.0525 V	3.3 V −7.5% tolerance	Meets logic thresholds
V_max	3.5475 V	3.3 V +7.5% tolerance	Verified by multimeter
I_nom	100 mA ±25 mA	ESP32 + sensors	Verified by load testing
I_peak	300 mA	Worst-case scenario	Verified via electronic load
Verification Process
Interface 1 (AC Input)

Connect barrel jack and AC adapter

Measure all three rails with multimeter

Confirm proper VRMS and current draw

Interface 2 (12 V Rail)

Tested under:

No load

Nominal load (250 mA)

Peak load (300 mA)

Using:

Electronic load set to desired current

Stepper and logic loads simulated using resistors

Interface 3 (5 V Rail)

Tested:

Idle (no load)

100 mA nominal load

300 mA peak load

Procedure includes electronic load + resistor simulations.

Interface 4 (3.3 V Rail)

Same testing structure as the 5 V rail with measurements taken at idle, nominal load, and peak load.

References

IEEE-formatted references maintained from original document:

(Your reference list remains unchanged but is fully compatible with README formatting.)

If you'd like, I can also generate:

✅ A cleaner GitHub-style version with badges and a project description
✅ A diagram of the power flow (text-based or rendered)
✅ A shorter executive-summary README
✅ A separate DESIGN.md, VALIDATION.md, and INTERFACES.md
