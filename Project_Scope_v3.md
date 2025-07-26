# Project Scope: ESP32 Badminton Court Timer v3

## 1. Project Overview

The objective is to create a robust and user-friendly timer system for a badminton court using an ESP32 microcontroller. The system is controlled via a responsive web page, accessible from any device on the same local WiFi network. The ESP32 manages all game timing, including game duration and break periods, and activates a relay connected to a siren to signal key events. This version of the project includes significant architectural improvements for stability, accuracy, and user experience.

## 2. Core Features

*   **Web-Based Control Panel:** A user-friendly, responsive web interface to control the timer. The UI is optimized for both desktop and mobile devices.
*   **Easy Network Access (mDNS):** Access the control panel via a friendly name (`http://badminton-timer.local`) instead of an IP address.
*   **Real-Time Clock with Automatic DST:**
    *   Displays the current time on the web interface, synchronized from an NTP server.
    *   **Improvement:** Automatically adjusts for New Zealand Daylight Saving Time.
*   **Robust WiFi Connectivity:**
    *   The ESP32 connects to an existing local WiFi network to serve the web page.
    *   **Improvement:** If the saved network is unavailable, it creates a fallback Access Point (`BadmintonTimerSetup`) with a captive portal for easy reconfiguration.
*   **Timer Controls:**
    *   **Start Button:** To start the entire match sequence.
    *   **Pause/Resume Button:** To temporarily pause and resume all timers.
    *   **Stop/Reset Button:** To stop the sequence and reset the system to its initial state.
*   **Configurable & Persistent Settings:**
    *   All settings are configured on the web page and saved to the ESP32's non-volatile memory, persisting through reboots.
    *   Settings include: Game Duration, Break Duration, Number of Rounds, Siren Length, and Siren Pause.
*   **Real-Time Display:**
    *   **Main Timer:** Displays the time remaining in the current round.
    *   **Break Durationr:** A smaller, secondary display showing the countdown of the break period.
    *   **Round Counter:** Displays the current round (e.g., "Round 1 of 3").
    *   **Improvement:** The countdown timers are animated smoothly in the browser using `requestAnimationFrame` to provide a perfect, non-jerky display.
*   **Visible Status Indicator:** A prominent GUI element changes color to indicate the current state:
    *   **Neutral:** The system is idle or the match is finished.
    *   **Green:** The match is active and a round is in progress.
*   **Audible Alarm:**
    *   The ESP32 controls a relay connected to a siren with distinct signals.
    *   **Improvement:** The siren is controlled via a non-blocking mechanism, ensuring the web server and UI remain responsive while the siren is active.

## 3. Hardware Requirements

*   ESP32 Development Board
*   5V Single-Channel Relay Module
*   External Siren/Buzzer
*   Appropriate Power Supplies for the ESP32 and siren.
*   Jumper Wires

## 4. Software Architecture

The project is built using the Arduino framework for the ESP32 and managed with PlatformIO.

*   **ESP32 Firmware (C++/Arduino):**
    1.  **WiFi & Settings Manager:** Uses the `ESPAsyncWiFiManager` library to handle all WiFi connectivity, including the configuration portal.
    2.  **Time Client:** Uses the `ezTime` library to manage time synchronization and automatic Daylight Saving Time adjustments for the "Pacific/Auckland" timezone.
    3.  **Web Server:** An asynchronous web server (`ESPAsyncWebServer`) serves the web interface and handles WebSocket connections for real-time, bidirectional communication.
    4.  **mDNS Service:** Broadcasts the `badminton-timer.local` address for easy access.
    5.  **Core Timer Logic:** A non-blocking state machine manages the timer's state (IDLE, RUNNING, PAUSED, FINISHED) and timing sequences.
    6.  **GPIO Control:** The firmware manages the relay pin with a non-blocking `millis()`-based function to avoid halting the processor.

*   **Web Interface (HTML/CSS/JavaScript):**
    1.  **HTML:** Provides the structure for all UI elements.
    2.  **CSS:** Styles the interface for a clean, responsive, and user-friendly experience on all screen sizes.
    3.  **JavaScript:**
        *   Manages the WebSocket connection for real-time data exchange.
        *   Handles all user interactions (button clicks, settings changes).
        *   Implements a smooth, client-side countdown animation using `requestAnimationFrame` that syncs with the ESP32's authoritative state.

## 5. Project Documentation

*   **README.md:** A comprehensive document is included in the project, detailing the features, setup instructions, and usage guide.
