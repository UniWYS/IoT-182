# Rainmaker Example

## Build and Flash firmware

Follow the ESP RainMaker Documentation [Get Started](https://rainmaker.espressif.com/docs/get-started.html) section to build and flash this firmware. Just note the path of this example.

## What to expect in this example?

- This example uses the BOOT button ESP32-S3-DevKitC board to demonstrate reading from multiple sensors and controlling of LED strip.
- The LED state can be edited on the Rainmaker App.
- Pressing the BOOT button will reinitialize the Wi-Fi Credential. This will also reflect on the phone app.
- Toggling the button on the phone app should toggle the LED on your board, and also print messages like these on the ESP32-S3 monitor:

```
I (16073) app_main: Received value = true for LED power - power
```

### LED not working?

In this example, the LED pin is connected to GPIO 19. Please make changes to `LED_STRIP_BLINK_GPIO` for your case.

### Reset to Factory

Press and hold the BOOT button for more than 3 seconds to reset the board to factory defaults. You will have to provision the board again to use it.
