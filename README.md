# Water Control

## Objective

The objective of this project is to automate the control of a water pump system using one ESP32 Dev Kit and one ESP8266. The system consists of two sets of microcontrollers, each with distinct roles:

- **ASKER Set**: Composed of an ESP8266, this set controls a relay connected to a water pump. It publishes periodic MQTT message to a server to tell if the ESP8266 still alive or not and subscribe another topic that control the water pump. This set also have a pullup button to manually turn on or off the water pump. This action is equal an on/off action from server.
- **SENDER Set**: Composed of ESP32 Dev Kit, this set includes a two rain sensor plugged into a cables. When the water reaches a certain level in the tank, the sensor sends a signal to the ESP32, which then publishes the server to change the pump's status to "off". The other sensor is used to detect if water is coming to the water box.
- **SERVER Set**: A web server that receives MQTT messages from both the ASKER and SENDER sets. It processes these messages and sends commands back to the ASKER set to control the water pump based on the water level detected by the SENDER set.

The water pump fills a distant water tank. Once the tank is full and water reaches the liquid level sensor, a shutdown signal is sent by the SENDER ESP32 Dev Kit. The ASKER ESP8266 can receive an off or on signal from the server to control the relay and, consequently, the water pump.

## Challenges

Programming this system presented several challenges:

1 **Reliability of Wireless Communication**: Ensuring that the ESP8266 and ESP32 Dev Kit modules could maintain a reliable wireless connection and MQTT connection to the server for both sending and receiving status updates was another challenge. Any interruption in this communication could result in an inaccurate pump state, potentially causing overflow or water shortages.

2 **Synchronizing Multiple Microcontrollers**: Coordinating actions between two sets of microcontrollers (ASKER and SENDER) in different locations required careful synchronization to ensure the water pump operates correctly and the system responds promptly to changes in water level.

This project showcases the integration of various hardware components and the use of web server communication to automate a real-world task, providing an effective solution for remote water tank management.
