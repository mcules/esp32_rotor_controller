# ESP32 Rotor Controller

## Overview

This project is a WiFi-based rotor controller for an Azimuth-Elevation (Az/El) system, built on the ESP32-C3 platform. Clients can connect via TCP to control and monitor the rotor's position. The script uses the WiFiManager library for network configuration and provides various commands for interacting with the rotor.

In the future, a web interface will be added to configure and manage the controller more easily.

---

## Features

- **WiFi Connectivity**: Automatically connects to a WiFi network using WiFiManager.
- **TCP Server**: Listens for client commands on port `4533`.
- **Rotor Position Management**:
  - Query current Az/El position.
  - Set a target Az/El position.
  - Stop rotor movement.
- **Command Interface**: Supports specific commands to control and query the rotor.
- **Smooth Movement**: Simulates smooth rotor movements towards the target position.
- **Future Web Interface**: Planned addition of a web-based GUI for easier configuration and control.

---

## Hardware Requirements

- ESP32-C3 development board
- Azimuth-Elevation rotor (optional; for integration)
- WiFi network

---

## Software Requirements

- Arduino IDE or compatible ESP32-C3 programming environment
- WiFiManager library (`WiFiManager`)

---

## Setup

1. **Install Dependencies**:
   - Download and install the Arduino IDE.
   - Add ESP32 board support to the Arduino IDE (ensure ESP32-C3 compatibility).
   - Install the `WiFiManager` library through the Arduino Library Manager.

2. **Upload Code**:
   - Open the provided script in the Arduino IDE.
   - Connect your ESP32-C3 to the computer via USB.
   - Select the correct board and COM port in the Arduino IDE.
   - Upload the code to the ESP32-C3.

3. **Configure WiFi**:
   - On first boot, the ESP32-C3 will create a WiFi access point.
   - Connect to the AP and configure your WiFi credentials using the WiFiManager portal.

4. **Run**:
   - After WiFi configuration, the ESP32-C3 will connect to the specified network and start the TCP server on port `4533`.

---

## Commands

The ESP32-C3 accepts the following commands via the TCP interface:

| Command               | Description                                                                                   | Example Input          |
|-----------------------|-----------------------------------------------------------------------------------------------|------------------------|
| `/p`                  | Get the current position of the rotor (Azimuth and Elevation).                                | `/p`                  |
| `/P <Azimuth> <Elevation>` | Set the target position for the rotor. Coordinates should be in degrees.                  | `/P 45.0 30.0`         |
| `/S`                  | Stop rotor movement and hold the current position.                                           | `/S`                  |
| `/_`                  | Query the model name of the rotor controller.                                                | `/_`                  |
| `/q`                  | Disconnect the client.                                                                       | `/q`                  |
| `/dump_state`         | Get the rotor's configuration, including min/max Az/El and other parameters.                 | `/dump_state`         |

---

## Code Highlights

### Functions

- `getRotorCurrentPosition()`: Returns the current rotor position in a formatted string.
- `setRotorPosition(azimuth, elevation)`: Sets the target position for the rotor.
- `stopRotor()`: Stops rotor movement and holds its current position.
- `getDumpState()`: Returns rotor configuration information.
- `processCommand()`: Parses and executes incoming commands from a client.
- `updatePosition()`: Simulates the smooth transition of the rotor towards its target position.

### Networking

- The server listens on port `4533` for incoming TCP connections.
- Multiple clients are supported, and their commands are processed sequentially.

---

## Customization

- **Position Update Interval**: Adjust the `POSITION_UPDATE_INTERVAL` (in milliseconds) for smoother or faster position updates.
- **Port Number**: Change the `TCP_SERVER_PORT` to modify the server's listening port.
- **Movement Speed**: Modify the increment/decrement values in `updatePosition()` to control how quickly the rotor moves to its target position.

---

## Planned Web Interface

A web interface will be added in future updates to:
- [ ] Configure network settings.
- [ ] Configure rotor settings.
- [x] Monitor rotor status and position.
- [x] Send commands directly through a browser.
- [ ] Offer an intuitive GUI for managing the rotor.

---

## Troubleshooting

- **WiFi Not Connecting**: Ensure your network credentials are entered correctly during the WiFiManager setup.
- **No Response to Commands**: Verify that the client is connected to the correct IP and port of the ESP32-C3.
- **Unstable Movement**: Check for issues in the `updatePosition()` logic or refine the movement parameters.

---

## Future Improvements

- Add actual rotor hardware integration for real-world applications.
- Include feedback from encoders or sensors for accurate positioning.
- Build and integrate the planned web interface.

---

Happy Hacking!
