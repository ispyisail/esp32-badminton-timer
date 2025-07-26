# ESP32 Badminton Court Timer

This project transforms an ESP32 microcontroller into a sophisticated, web-controlled timer specifically designed for badminton courts. It provides a clear, responsive user interface that can be accessed from any device with a web browser on the same local network, such as a smartphone, tablet, or laptop.

The timer is ideal for clubs, training facilities, or personal use, ensuring fair and consistent timing for games and practice sessions.

## Features

*   **Web-Based Interface:** Control the timer from any device with a web browser. No app installation required.
*   **Responsive UI:** The user interface is designed to work on both large screens and mobile devices.
*   **Game and Break Timers:** Independent countdown timers for game time and break periods.
*   **Configurable Rounds:** Easily set the number of rounds for a match.
*   **Real-Time Clock:** Displays the current time, automatically synchronized from the internet.
*   **Automatic Daylight Saving Time:** The clock automatically adjusts for New Zealand Daylight Saving Time.
*   **Non-Blocking Siren:** An external relay/siren is controlled without freezing the device, ensuring the web interface remains responsive at all times.
*   **WiFi Configuration Portal:** If the device can't connect to its saved WiFi network, it automatically creates a setup network (`BadmintonTimerSetup`) with a captive portal to allow for easy WiFi configuration.
*   **Persistent Settings:** All timer settings (game duration, break duration, etc.) are saved to non-volatile memory and are retained through power cycles.
*   **Over-the-Air (OTA) Updates:** The firmware can be updated wirelessly over the local network, protected by a password.

## Hardware Requirements

*   ESP32 Development Board
*   A relay module to control an external siren or light.
*   An external siren, buzzer, or light.

## Software Setup

This project is built using the PlatformIO IDE.

### Dependencies

The following libraries are required and are managed automatically by PlatformIO via the `platformio.ini` file:

*   `bblanchon/ArduinoJson`
*   `alanswx/ESPAsyncWiFiManager`
*   `ezTime`

### Build and Upload Instructions

1.  **Open the Project:** Open this project folder in Visual Studio Code with the PlatformIO IDE extension installed.

2.  **Build the Firmware:** Build the main program firmware by running the following command in the PlatformIO CLI terminal:
    ```bash
    platformio run
    ```

3.  **Upload the Firmware:** Connect the ESP32 to your computer via USB and run the following command to upload the firmware:
    ```bash
    platformio run --target upload
    ```

4.  **Build and Upload the Filesystem:** The web interface files (HTML, CSS, JavaScript) are stored in the `data` directory. They need to be uploaded to the ESP32's SPIFFS (SPI Flash File System). Run the following command to do this:
    ```bash
    platformio run --target uploadfs
    ```

## First-Time Use and WiFi Configuration

1.  **Power On:** Power on the ESP32.
2.  **Connect to Setup Network:** On your phone or computer, look for a WiFi network named `BadmintonTimerSetup` and connect to it.
3.  **Captive Portal:** Your device should automatically open a web page. If it doesn't, open a web browser and navigate to `http://192.168.4.1`.
4.  **Configure WiFi:** On the web page, select your local WiFi network (SSID) from the list, enter the password, and click "Save".
5.  **Connect:** The ESP32 will save the credentials, restart, and connect to your local network. The `BadmintonTimerSetup` network will disappear.

## Accessing the Timer

Once the ESP32 is connected to your local network, you can access the timer's web interface from any device on the same network by navigating to:

**http://badminton-timer.local**

You can also use the IP address of the ESP32 if you know it.

## Over-the-Air (OTA) Updates

This project supports OTA updates. Once the initial firmware is uploaded via USB, you can upload subsequent versions over the network.

*   **OTA Hostname:** `badminton-timer.local`
*   **OTA Password:** `badminton`

**Security Note:** The OTA password is hardcoded in the `src/main.cpp` file. For a personal project on a secure home network, this is generally acceptable. However, if you plan to deploy this in a more public or untrusted environment, it is highly recommended that you change this password to something more secure.
