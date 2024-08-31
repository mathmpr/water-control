# Water Control

## Objective

The objective of this project is to automate the control of a water pump system using two Arduino boards and two ESP-01 modules. The system consists of two sets of microcontrollers, each with distinct roles:

- **ASKER Set**: Composed of an Arduino and an ESP-01, this set controls a relay connected to a water pump. It sends periodic requests to a web server to check the status of the pump (whether it should be on or off).
- **SENDER Set**: Composed of another Arduino and an ESP-01, this set includes a liquid level detector. When the water reaches a certain level in the tank, the sensor sends a signal to the ESP-01, which then updates the server to change the pump's status to "off".

The water pump fills a distant water tank. Once the tank is full and water reaches the liquid level sensor, a shutdown signal is sent by the SENDER ESP-01. The ASKER ESP-01 continuously queries the server for the pump's status. If the status is `false` (off), the relay is deactivated, stopping the water pump.

## Challenges

Programming this system presented several challenges:

1. **Serial Communication Issues**: The serial communication between the ESP-01 and Arduino Uno modules led to memory overflow problems. Managing the data transmission without overloading the limited memory of the Arduino Uno was challenging.

2. **Memory Constraints**: The Arduino Uno's limited memory posed a significant restriction. Due to these constraints, it wasn't feasible to transmit complex data structures such as JSON strings over serial communication. Efficiently managing memory usage while ensuring reliable data transmission was a key challenge that required careful consideration and optimization.

3. **Reliability of Wireless Communication**: Ensuring that the ESP-01 modules could maintain a reliable wireless connection to the server for both sending and receiving status updates was another challenge. Any interruption in this communication could result in an inaccurate pump state, potentially causing overflow or water shortages.

4. **Synchronizing Multiple Microcontrollers**: Coordinating actions between two sets of microcontrollers (ASKER and SENDER) in different locations required careful synchronization to ensure the water pump operates correctly and the system responds promptly to changes in water level.

This project showcases the integration of various hardware components and the use of web server communication to automate a real-world task, providing an effective solution for remote water tank management.
