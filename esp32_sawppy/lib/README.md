# Components for ESP32 Sawppy brain

Organized under the following sections:

* `hardware` - Components that interface with ESP32 hardware peripherals,
with the exception of WiFi which has its own directory.
* `messages` - Data message formats for components to communicate with each other.
* `rover` - Rover-specific concepts with little ESP32-specific code. Should be
easy to port to platforms other than ESP32.
* `wifi` - Serving HTML-based rover user interface over ESP32 WiFi.
HTML/CSS/JavaScript files are pulled from elsewhere in this repository.
