## RFID-ATTENDENCE-SYSTEM---LPC1768
<img width="980" height="540" alt="image" src="https://github.com/user-attachments/assets/e9d4da16-0067-48cb-b40e-871eaaf91eac" />

📌 RFID Based Attendance System Using LPC1768 (ARM Cortex-M3)
📝 Project Overview

The RFID Based Attendance System using LPC1768 ARM Cortex-M3 is an embedded system designed to provide secure, automated, and time-restricted attendance monitoring.
Attendance is marked only for registered RFID cards and only during a predefined time period.

All system outputs and attendance status are displayed through QCOM Serial Monitor, with no LCD interface used.

🎯 Objectives

Automate attendance using RFID technology

Allow attendance only for registered users

Restrict attendance to a specific allowed time window

Prevent unauthorized or late attendance entries

Display system output via QCOM serial communication

🛠️ Hardware Components

LPC1768 ARM Cortex-M3 Microcontroller

RFID Reader Module

RFID Cards/Tags

Power Supply

USB-to-Serial (for QCOM monitoring)

Connecting Wires

💻 Software Tools

Embedded C

Keil µVision IDE

Flash Magic

QCOM Serial Monitor

⚙️ System Working Principle

The RFID reader scans the RFID card.

The card’s unique ID is sent to the LPC1768 microcontroller.

The controller verifies:

Whether the card is registered

Whether the scan time falls within the allowed attendance time

Based on verification, the result is displayed on QCOM.

⏰ Attendance Logic
✅ Case 1: Registered Card + Allowed Time

Attendance is successfully marked

QCOM Output:

RFID Detected
Student Verified
Attendance Marked Successfully

❌ Case 2: Registered Card + Outside Allowed Time

Attendance NOT marked

QCOM Output:

RFID Detected
Student Verified
ACCESS DENIED – Invalid Time

❌ Case 3: Unregistered Card

Attendance NOT marked

QCOM Output:

RFID Detected
ACCESS DENIED – Unauthorized Card

📊 Results
✅ Observed Output (QCOM Only)

Attendance recorded only for valid users

Time-based restriction working accurately

Unauthorized cards completely blocked

No duplicate or invalid entries allowed
