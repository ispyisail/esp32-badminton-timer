# Installation Guide: ESP32 Badminton Court Timer

This guide provides a complete, step-by-step process for assembling the hardware and installing the software for the ESP32 Badminton Court Timer.

## Part 1: Hardware Assembly

### Required Components

| Quantity | Component                  | Model (Example) | Purpose                                  |
| :------- | :------------------------- | :-------------- | :--------------------------------------- |
| 1        | ESP32 Development Board    | XC3800          | The core microcontroller of the project. |
| 1        | 5V Single-Channel Relay    | XC4419          | To safely switch the high-power siren.   |
| 1        | External Siren or Buzzer   | -               | The audible alarm.                       |
| 1        | 5V USB Power Supply        | -               | To power the ESP32.                      |
| 1        | Separate Power Supply      | -               | To power the siren (match its voltage).  |
| 3+       | Jumper Wires (Dupont)      | -               | To connect the components.               |
| 1        | Project Enclosure (Optional) | -             | To safely house the electronics.         |

### Wiring Diagram

Connect the ESP32 to the relay module as follows. **Ensure the ESP32 is disconnected from power while wiring.**

| ESP32 Pin | Relay Module Pin |
| :-------- | :--------------- |
| `GND`     | `GND` or `-`     |
| `5V`      | `VCC` or `+`     |
| `GPIO 26` | `IN` or `S`      |

**Relay and Siren Connection:**

The relay has two sides: a low-power control side (connected to the ESP32) and a high-power switch side. The switch side has three terminals:

*   **COM:** Common
*   **NO:** Normally Open
*   **NC:** Normally Closed

You will wire the relay to interrupt the power supply to the siren.

1.  Connect the **positive (+)** wire from your siren's power supply to the **COM** terminal on the relay.
2.  Connect the **positive (+)** wire from your siren to the **NO** (Normally Open) terminal on the relay.
3.  Connect the **negative (-)** wire from your siren's power supply directly to the **negative (-)** wire of your siren.

This way, when the ESP32 activates the relay, the `COM` and `NO` terminals are connected, completing the circuit and sounding the siren.

## Part 2: Software Installation

This project is built using **PlatformIO**. The recommended way to install the software is with Visual Studio Code.

### Prerequisites

1.  **Install Visual Studio Code:** Download and install it from the [official website](https://code.visualstudio.com/).
2.  **Install PlatformIO IDE Extension:** Open VS Code, go to the "Extensions" view (Ctrl+Shift+X), search for `PlatformIO IDE`, and install it.

### Installation Steps

1.  **Download the Project:** Download the project files (or clone the repository) to your computer.

2.  **Open in PlatformIO:** In VS Code, go to the "PlatformIO" section in the activity bar (the alien head icon). Click "Open Project" and select the downloaded project folder.

3.  **Build the Firmware:** Open a new PlatformIO CLI terminal by clicking the "Terminal" icon in the PlatformIO toolbar. In the terminal, run the following command to build the project and download all the necessary libraries:
    ```bash
    platformio run
    ```

4.  **Upload the Firmware:** Connect your ESP32 to your computer via USB. PlatformIO should automatically detect the correct COM port. Run the following command to upload the main program:
    ```bash
    platformio run --target upload
    ```

5.  **Upload the Web Interface:** The files for the web interface are located in the `data` directory. They must be uploaded to the ESP32's internal flash storage (SPIFFS). Run the following command:
    ```bash
    platformio run --target uploadfs
    ```

## Part 3: First-Time Use

1.  **Power On:** Power on the ESP32 using its 5V USB power supply.
2.  **Connect to Setup Network:** On your phone, tablet, or computer, search for a new WiFi network named **`BadmintonTimerSetup`** and connect to it.
3.  **Configure WiFi:** Your device should automatically open a "captive portal" web page. If it doesn't, open a web browser and go to `http://192.168.4.1`.
4.  **Select Your Network:** On the portal page, click "Configure WiFi," select your local WiFi network (SSID) from the list, enter its password, and click "Save."
5.  **Connect:** The ESP32 will restart and connect to your local network. The `BadmintonTimerSetup` network will now disappear.

## Part 4: Using the Timer

Once the ESP32 is on your network, you can access the timer from any device on the same network.

1.  Open a web browser.
2.  Navigate to the following address: **http://badminton-timer.local**

You should now see the timer interface and be able to control it.

## Part 5: Over-the-Air (OTA) Updates

After the initial USB upload, you can update the firmware wirelessly.

*   **OTA Hostname:** `badminton-timer.local`
*   **OTA Password:** `badminton`

To do this, modify the `platformio.ini` file to specify the upload port as the device's mDNS address and set the upload protocol to `espota`.

**Security Warning:** The OTA password is set in the source code (`src/main.cpp`). If you are deploying this in a public environment, you should change this password to a more secure one.
