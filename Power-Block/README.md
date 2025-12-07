#1.1. Power Block - Andrew Gondoputro

This block is located on the exterior input of the system, accepting standard U.S. wall power and converting it into a regulated low-voltage DC bus at several predetermined voltages that all other M.E.O.W. subsystems can utilize. Every other block in the system depends on this stable power for its parts to function and is in charge of isolation, overcurrent protection, and providing enough power for the other components in the system.

#1.1.1. Description

The Power Distribution Block takes the AC_pwr_in interface with the US Standard 120 VAC, 60 Hz from a U.S. wall outlet. This allows us to plug into a wall and have a consistent power source, unlike a battery. Internally, the block then takes this AC wall power and turns it into a 12V rail, for higher voltage operations on the system like the kinematics of moving the stepper motors. It then uses that 12V rail to step down into both a 5V rail and a 3.3V rail, used for lower voltage components like the microcontroller, stepper drivers and sensors.

#1.1.2. Design

The design of the power station first starts with connecting to a wall outlet. Standard NEMA US Connectors consist of two pinned enclosures. We decided to have the cleanest AC->DC conversion to use a purchasable wall mount 12V 2A adapter with a barrel jack connector [10]. This also allows the cable of our overall system to be modular in which you can disconnect from the system and also easily replace.

Starting our circuit, we designed a barrel jack connector as an input for just a power and gnd, making sure that the footprint was right on the schematic. From the power pin A we put a Schmitt trigger diode so we would not get any accidental reverse current. The next node from the diode would just be used as the 12 V rail for the system since it was already cleaned by the adapter.

We decided to use two-buck conversions for the least amount of heat problems. We went with the LM2596S because of its easy accessibility of purchasing and consistent documentation and usages. The system then uses a 470uF and 1uF capacitor to deal with any low/high frequency noise. Our system uses two LM2596S taking the 12V input, with one LM2596S downstepping to 5V and the other downstepping to 3.3V. Following the example of [3, pg 1] we looked for capacitors and inductors and the wiring of the buck converters. Tables 9.1 and 9.3 [3, pg 21] showed us which inductors and capacitors to use. Which can be seen in our schematic [Figure 3.1.1]. The outputs of the buck converters gave us our rails we would be using.

Moving onto our PCB we that we decided that our project would later become open source, we tried to use a lot of through-hole components for easier assembly. We spent a long time looking for footprints and then making 1.5mm traces for high-power components. The pcb layout ended up being about the size of a credit card and can be seen in Figure 3.1.2. We then used screw terminals for the outputs of the rails for testing purposes when creating the PCB.

#Design Artifacts

Figure 3.1.1 KiCAD Schematic
Figure 3.1.2 KiCAD PCB Trace Schematic
Figure 3.1.2 KiCAD PCB 3D Model

#1.1.3. General Validation

It uses an AC to DC 12V power adapter to get the first regulated rail. It then uses a DC Voltage regulator to convert that 12V DC to a single 5 V DC regulated output that is used by the other blocks. Internally, this 5V is then fed into another voltage regulator to get a single 3.3 V DC regulated output for a microcontroller. This design aligns with the top-level system plan, which calls out a dedicated power block alongside enclosure/kinematics, sensors, microcontroller, code/database, and UI.

#1.1.4. Interface Validation

Interface property
Be sure to use the naming convention “from_to_type” here.
Why is this interface property this value?
How do you know your design details will meet or exceed this property? Cite your sources in IEEE.

**Interface 1: ext_power_acpwr**
| Interface property                                 | Why is this interface property this value?                                                                       | How do you know your design details will meet or exceed this property?                                                   |
| -------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| **V_rms = 120 VAC @ 60 Hz (wall_pwr_pwr_ac)**      | Section 1 and 2 say “The system will use standard US wall.” U.S. residential mains are 120 VAC nominal at 60 Hz. | The selected AC-DC module is rated for 90–132 VAC input at 47–63 Hz, so 120 VAC/60 Hz is inside range. [10]              |
| **I_in_max ≤ 0.5 A (wall_pwr_pwr_ac)**             | A 15 W / 5 V / 3 A supply at 120 VAC draws ~0.17 A even at poor PF.                                              | Adapter rating guarantees operation below this current. [10]                                                             |
| **Connector = 2-pin shielded / enclosure mounted** | Since we take wall power, this is the easiest AC→DC form factor.                                                 | Using an IEC/2-pin inlet with strain relief meets electrical and mechanical requirements; it is also the purchased part. |
| **Barrel Connector Between PCB and Outlet**        | Typical devices use a separate connector so the cord is removable.                                               | 2.1 mm male/female barrel jacks are used. [10]                                                                           |

**Interface 2. power_enc_dcpwr12v**
| Interface property                 | Why is this interface property this value?                                    | How do you know your design meets this property?                                |
| ---------------------------------- | ----------------------------------------------------------------------------- | ------------------------------------------------------------------------------- |
| **V_min = 11.1 V (pwr_all_dcpwr)** | 12 V ±7.5% allows the buck converter and stepper motors to operate correctly. | Stepper motor is rated 8–35 V, well within range. [7]                           |
| **V_max = 12.9 V (pwr_all_dcpwr)** | Same tolerance requirement as above.                                          | Stepper motor and converters operate fully across this range. [7]               |
| **I_nom = 250 mA**                 | Expected slow-movement draw of the motor.                                     | LM2596S converter maintains regulation at no load and normal load. [3]          |
| **I_peak = 300 mA**                | Ensures startup + acceleration overhead is supported.                         | Stepper motors typically require ≤ 300 mA under these operating conditions. [7] |

**Interface 3. dc_pwr_out_5v**

| Interface property     | Why is this interface property this value?                  | How do you know your design meets/exceeds it?                                        |
| ---------------------- | ----------------------------------------------------------- | ------------------------------------------------------------------------------------ |
| **V_min = 4.625 V**    | 5 V −7.5% is USB and logic lower tolerance limit.           | LM2596S guarantees ±10% output; verified by open-circuit tests. [3]                  |
| **V_max = 5.375 V**    | 5 V +7.5% keeps downstream devices safe.                    | LM2596S ±5% accuracy meets this requirement.                                         |
| **I_nominal = 100 mA** | Matches typical load cells and sensor current requirements. | ESP32 quiescent current = 5 mA; total expected load = 105 mA < nominal limit. [2][6] |
| **I_peak = 300 mA**    | Supports transient demands.                                 | Load remains below converter limit; ESP32 remains far under this threshold.          |

Interface 4. dc_pwr_out_3.3v
| Interface property             | Why is this interface property this value?                | How do you know your design meets/exceeds it?                       |
| ------------------------------ | --------------------------------------------------------- | ------------------------------------------------------------------- |
| **V_min = 3.0525 V**           | 3.3 V −7.5% meets logic tolerance for ESP32/sensors.      | LM2596S regulation ensures output stays above this even under load. |
| **V_max = 3.5475 V**           | 3.3 V +7.5% stays within max voltage for logic chips.     | Verified by open-circuit measurement ≤ 3.465 V. [3]                 |
| **I_nominal = 100 mA ± 25 mA** | Normal operating range for microcontroller + peripherals. | ESP32 quiescent is 5 mA; typical load ≤ 105 mA.                     |
| **I_peak = 300 mA**            | Allows for transient inrush conditions.                   | Remains well below LM2596S converter limits.                        |

#1.1.5. Verification Process
Interface 1: ext_power_acpwr

Barrel Jack Connector and 2-pin shielded connector

Connect the cable to the system’s PCB barrel jack connector

Plug the system’s wall connector into the wall
VRMS (NEMA Standard) and Iinmax (Wall Adapter)

Use a multimeter on the rails to check the voltage of each rail receiving their expected values

Fully verify the interfaces 2-4 validation’s to ensure its working'

Interface 2: power_enc_dcpwr12v

11.1V < Vout < 12.9V and Imin= 0 (Verifying when motor off)

Disconnect any loads on the 12V rail

While the system is connected to wall power, use a multimeter to verify that the values are within the range

11.4V < Vout < 12.6V when Imax= 250 mA (Verifying when motor on)
3. Have the system disconnected from the wall
4. Put the electronic load on the 12V rail with power and gnd properly wired
a. Set the load to I = 0.3 and then turn it on
b. Put a resistor value of 22 ohms on the 5V Rail (Simulating I = 227mA)
c. resistor value of 22 ohms on the 3V Rail (Simulating I = 150mA)
5. Plug the system into the wall
6. Check the electronic load to see the actual current and the voltage across the node to verify its within the values

11.4V < Vout < 12.6V when Imax = 300 mA (Verifying under max load range)

Unplug the system

Change electronic load to I=0.3

Plug the system back into the wall

Check the electronic load to see the actual current and the voltage across the node to verify its within the values

Interface 3: power_sensors_dcpwr5v

4.625 V < Vout < 5.375 V
5. Disconnect any loads on the 5V rail
6. While the system is connected to wall power, use a multimeter to verify that the values are within the range

4.625 V < Vout < 5.375 V when Inom = 200 mA (Verifying under nominal load range)
7. Have the system disconnected from the wall
8. Put the electronic load on the 12V rail with power and gnd properly wired
a. Set the load to I = 0.1 and then turn it on
b. Put a resistor value of 47 ohms on the 12V Rail (Simulating I = 255mA)
c. resistor value of 22 ohms on the 3V Rail (Simulating I = 150mA)
9. Plug the system into the wall
10. Check the electronic load to see the actual current and the voltage across the node to verify its within the values

4.625 V < Vout < 5.375 V when Imax = 300 mA (Verifying under max load range)
11. Unplug the system
12. Change electronic load to I=0.3
13. Plug the system back into the wall
14. Check the electronic load to see the actual current and the voltage across the node to verify its within the values

Interface 4: power_enc_dcpwr3.3v

3.05 V < Vout < 3.55 V (Verifying when system idle)

Disconnect any loads on the 12V rail

While the system is connected to wall power, use a multimeter to verify that the values are within the range

3.05 V < Vout < 3.55 V when Inom = 200 mA (Verifying under nominal load range)
3. Have the system disconnected from the wall
4. Put the electronic load on the 12V rail with power and gnd properly wired
a. Set the load to I = 0.1 and then turn it on
b. Put a resistor value of 47 ohms on the 12V Rail (Simulating I = 255mA)
c. resistor value of 22 ohms on the 5V Rail (Simulating I = 227mA)
5. Plug the system into the wall
6. Check the electronic load to see the actual current and the voltage across the node to verify its within the values

3.05 V < Vout < 3.55 V when Imax = 300 mA (Verifying under max load range)
7. Unplug the system
8. Change electronic load to I=0.3
9. Plug the system back into the wall
10. Check the electronic load to see the actual current and the voltage across the node to verify its within the values

1.1.6. References

(All references preserved exactly from the PDF)

[1] Americord Custom Power Cords, “NEMA Chart: Know Your Plug And Receptacle,” Americord Blog, Jul. 24, 2021.
[2] ON Semiconductor, “NCP1117/D: 1.0 A Low-Dropout Positive Fixed and Adjustable Voltage Regulators.”
[3] Texas Instruments / LM2596S Evaluation Module, “LM2596S EV Board Datasheet,” Digi-Key, 2020.
[4] Cliff Electronic Components Ltd., “Model No. DC-10A / DC-10B – Part No. FC68148.”
[5] Espressif Systems, “ESP32-WROOM-32 Datasheet.”
[6] SparkFun Electronics, “ESP32 Datasheet (Alternate Copy).”
[7] Allegro MicroSystems, “A4988: Microstepping Motor Driver with Translator.”
[8] Rubycon Corp., “YXJ Series Aluminum Electrolytic Capacitors.”
[9] Rubycon Corp., “ZLH Series Aluminum Electrolytic Capacitors.”
[10] Yageo Group, “12V 2A Power Supply.”
[11] Vishay Semiconductors, “1N5820 Schottky Barrier Rectifier.”
[12] Würth Elektronik, “744731680 – WE-PD Series Shielded Power Inductor.”
[13] Würth Elektronik, “7447471470 – WE-HCI Series High-Current Inductor.”
[14] Texas Instruments, “LM2596: SIMPLE SWITCHER® 150-kHz 3-A Step-Down Voltage Regulator.”
