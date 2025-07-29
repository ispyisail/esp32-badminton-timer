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
*   **Flexible WiFi Configuration:** Configure WiFi using the built-in captive portal or by hardcoding credentials directly in the firmware.
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

## WiFi Configuration

There are two methods to configure the WiFi connection for the ESP32 timer.

### Using the Captive Portal

This is the recommended method for most users.

1.  **Power On:** Power on the ESP32.
2.  **Connect to Setup Network:** On your phone or computer, look for a WiFi network named `BadmintonTimerSetup` and connect to it.
3.  **Captive Portal:** Your device should automatically open a web page. If it doesn't, open a web browser and navigate to `http://192.168.4.1`.
4.  **Configure WiFi:** On the web page, select your local WiFi network (SSID) from the list, enter the password, and click "Save".
5.  **Connect:** The ESP32 will save the credentials, restart, and connect to your local network. The `BadmintonTimerSetup` network will disappear.

**Note:** Some mobile devices or operating systems may have trouble with captive portals. If you are unable to connect or see the configuration page, you may need to "forget" the `BadmintonTimerSetup` network on your device and try again, or use the hardcoding method below.

### Hardcoding Wi-Fi Credentials

If you have issues with the captive portal or prefer to have the WiFi settings pre-configured, you can hardcode them directly into the firmware.

1.  **Rename the Example File:** In the `src` directory, rename `wifi_credentials.h.example` to `wifi_credentials.h`.

2.  **Edit the File:** Open `src/wifi_credentials.h` and replace `"YourSSID"` and `"YourPassword"` with your actual WiFi network name and password.

    ```cpp
    // src/wifi_credentials.h

    #pragma once

    #define WIFI_SSID "YourSSID"
    #define WIFI_PASSWORD "YourPassword"
    ```

3.  **Upload the Firmware:** Build and upload the firmware to the ESP32 as described in the "Build and Upload Instructions" section. The device will now automatically connect to the specified WiFi network, bypassing the captive portal setup.

## Accessing the Timer

Once the ESP32 is connected to your local network, you can access the timer's web interface from any device on the same network by navigating to:

**http://badminton-timer.local**

You can also use the IP address of the ESP32 if you know it.

**Note on mDNS:** The `badminton-timer.local` address relies on the mDNS (Multicast DNS) protocol. Some routers or network configurations block mDNS traffic. If you are unable to access the timer using this address, you have a few options:

*   **Use the IP Address:** Find the IP address of the ESP32 from your router's DHCP client list and use that to access the web interface. You may be able to configure your router to assign a static (fixed) IP address to the timer to make this easier.
*   **Configure Router's DNS:** Some routers allow you to add local DNS entries. You could create an entry that maps `badminton-timer.local` to the timer's IP address.
*   **Enable mDNS:** Check your router's settings to see if there is an option to enable or allow mDNS (it may also be referred to as "Bonjour" or "Zero-configuration networking").

## Over-the-Air (OTA) Updates

This project supports OTA updates. Once the initial firmware is uploaded via USB, you can upload subsequent versions over the network.

*   **OTA Hostname:** `badminton-timer.local`
*   **OTA Password:** `badminton`

**Security Note:** The OTA password is hardcoded in the `src/main.cpp` file. For a personal project on a secure home network, this is generally acceptable. However, if you plan to deploy this in a more public or untrusted environment, it is highly recommended that you change this password to something more secure.
