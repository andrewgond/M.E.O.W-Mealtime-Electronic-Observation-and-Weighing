

---

# M.E.O.W. — Mealtime Electronic Observation & Weighing (WIP)

**An Open-Source Smart Pet Feeder with Integrated Weight Tracking**
*Senior Design Capstone — Oregon State University*
*Andrew Gondoputro
*Ahmad Alajami
*Craig Kelley

---

## 📌 Project Status: Work in Progress (WIP)

This repository documents the development of the **M.E.O.W. automated pet feeder**, a system that dispenses food and logs pet weight using load cells and RFID identification. The goal is to support long-term tracking of pet health trends such as obesity, malnutrition, or abnormal feeding behavior.

---

# 📖 Overview

The M.E.O.W. system is designed to:

* Run on standard wall power
* Measure pet weight within ±20 grams
* Dispense food within ±60 grams
* Log food and weight data within 10 seconds
* Store at least 10 days of logs
* Support up to 4 pets via RFID
* Allow user-configurable feeding parameters

Unlike typical auto-feeders, M.E.O.W. integrates *health-relevant data logging* for improved wellness monitoring.

---

# 🌍 DEI Commitment

The project emphasizes accessibility, inclusion, and open-source availability.
The system aims to be easy to use for any pet owner, with a simple UI and customizable configuration.

---

# 🏗️ System Architecture

## Top-Level Summary

The system is divided into modular subsystems:

* Power Distribution
* Enclosure & Kinematics
* Sensors
* Code & Database Storage
* Microcontroller (ESP32)
* UI + Web App

---

# 🔌 Block Diagram (Placeholder)

```
[ Block Diagram Placeholder — MEOW System ]
```

```
[ System-Level Diagram Placeholder — Showing Subsystem Connections ]
```

---

# 🔧 Subsystem Descriptions

### 1. Power Distribution

Converts AC wall power into stable low-voltage DC rails for sensors, logic, and actuators.

### 2. Enclosure & Kinematics

Includes the physical enclosure, motors, dispensing components, and protective mechanical structures.

### 3. Sensors

* Load cells to capture weight
* RFID reader for pet identification

### 4. Code & Database

MicroSD-based data storage for logs, timestamps, food dispensing events, and per-pet data.

### 5. Microcontroller

ESP32 manages sensors, actuator control, data logging, and UI communication.

### 6. UI + Web App

Browser-based dashboard for monitoring weight trends, adjusting feeding schedules, and visualizing logs.

---

# 🔌 Interface Definitions

| Interface           | Description                          |
| ------------------- | ------------------------------------ |
| AC_pwr_in           | Wall power input                     |
| DC_pwr_in_5v        | Regulated 5V power rail              |
| env_weight          | Load cell weight signal              |
| env_rfid_dist       | RFID read distance                   |
| if_user_commands_in | User command interface               |
| motor_ctrl_out      | Motor control signals                |
| ui_disp_ui          | Display output                       |
| db_data_out         | Data sent from database              |
| db_data_to_ui       | Data sent from microcontroller to UI |

---

# 🧪 Verification Framework

Each subsystem will include:

* Interface validation
* General validation
* Verification steps
* References
* Success criteria

Example tasks include:

* Maximum current draw calculations
* Logic level matching (ESP32 ↔ SD card)
* Regulator thermal margins
* Sensor precision/accuracy tests

---

# 📚 References

A `/docs/references.md` file will be added containing:

* Component datasheets
* Industry standards
* Research sources for pet feeding and behavior
* Market analysis references

---

# 🧩 Future Work

### Technical Recommendations (WIP)

* Improve sensor calibration algorithms
* Add multi-pet discrimination logic
* Enhance UI accessibility
* Expand SD logging features
* Improve enclosure materials

### Teamwork/Communication Recommendations

* Better task delegation strategy
* Implement weekly fixed-scope sprints

---

# 📂 Project Roadmap

### Phase 1 — Hardware Bring-Up

* [ ] Power subsystem validation
* [ ] Load cell calibration
* [ ] Servo/stepper testing
* [ ] RFID module bring-up

### Phase 2 — Firmware

* [ ] Sensor polling loops
* [ ] SD logging system
* [ ] Motor control logic
* [ ] Error handling

### Phase 3 — UI + Data

* [ ] Web dashboard
* [ ] Trend graphs
* [ ] Pet profiles

### Phase 4 — Final Integration

* [ ] Full hardware + firmware integration
* [ ] Enclosure finalization
* [ ] Stress testing
* [ ] Final documentation

---

# 🐾 License

To be decided (MIT / BSD / Apache).

---

# 📬 Contact

Project Contributors:

* Ahmad Alajmi
* Andrew Gondoputro
* Craig Kelley

---

If you want, I can also:

✅ Add GitHub badges
✅ Generate professional vector diagrams
✅ Create a `/docs` folder structure
✅ Auto-generate a Table of Contents
✅ Make a polished non-WIP final README when you're ready
