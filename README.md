<!--  preview use F1 -> Markdown preview -->

# ESPEnlightDALI

**Overview:**

ESPEnlightDALI is a high-performance library utilized the Espressif IoT Development Framework (ESP-IDF)
designed to facilitate DALI (Digital Addressable Lighting Interface) protocol communication for ESP8266 and ESP32 microcontrollers.
It serves as a low- and mid-level DALI driver, enabling seamless integration of DALI lighting control functionality into your IoT projects.

**Key Features:**

- **Platform Compatibility:** Compatible with ESP8266 and ESP32 microcontrollers, providing flexibility across a range of IoT applications.

- **Efficient Implementation:** Utilizes GPIO (General Purpose Input/Output) pins and hardware timer interrupts to ensure efficient and reliable communication with DALI lighting fixtures.

- **Scalability:** Designed to support scalable lighting control solutions, allowing for the management of multiple DALI devices within a network.

- **Optimized Performance:** Optimized for performance and responsiveness, enabling real-time control and monitoring of DALI lighting systems.



**Additional Features in Detail:**
- The library supports sending and receiving frames of length 1 up to 32 bits, and optionally more, with a configurable maximum length.
- The library allows dynamic debug mode, where precise timing of all delivered frames can be monitored at any time. For each bit, or level change on the DALI bus, the duration since the last change is determined, along with the interpretation of the value, indicating whether it is a bit 0 or 1.
- In debug mode, for the transmitted frames, the delay between the rising edge on the TX interface and its detection on the RX interface is additionally monitored to assess the quality of the DALI interface. Sometimes there is a significant difference in delay between the rising and falling edges. Poor interface design can cause the overall interface to fail to meet protocol conditions.
- The library includes a simple UART communication for performing all common DALI device settings, such as device discovery, assigning long and short addresses,
changing short addresses, setting and retrieving configuration parameters for levels, groups and scenes as well as sending custom commands.
- The library should theoretically support all standard and non-standard protocol commands according to the IEC 62386 standard.

**Tested Hardware:**

- **ESP8266 D1 Mini:**
  - **Crystal Frequency:** 26MHz
  - **Memory:** 4MB
  - Note: Not all ports on the ESP may be used as outputs, and on some, interrupts cannot be activated.
    - For frame reception, select an RX port that supports GPIO interrupts.
    - For the TX output port, choose one that can be configured as an output.

- **DALI interface:**
  - Available was a DALI interface that barely met the conditions of DALI version 1. However, the library is aimed to support timing, collision detection, and recovery as per DALI 2 standards.


**Usage:**

- To integrate ESPEnlightDALI into your ESP8266 or ESP32 ESP-IDF project, simply include the library in your development environment and utilize its intuitive API to initialize, configure, and control DALI lighting fixtures.
