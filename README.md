# T-Display-S3 NyanCat Clock

An animated clock for the LilyGO T-Display-S3 featuring:
- Nyan Cat animation
- NTP time synchronization
- Adjustable brightness controls
- Performance monitoring (FPS counter)
- Network connection info

## Features

- Clock synchronized via NTP
- Animated Nyan Cat display
- Hardware button brightness control
- WiFi connection status with IP display
- FPS counter for performance monitoring
- Date and weekday display
- Configurable timezone support

## Pin Configuration

| Function       | GPIO Pin |
|----------------|----------|
| Left Button    | 0        |
| Right Button   | 14       |
| TFT Backlight  | TFT_BL   |

## Notes

- First build the project in PlatformIO to download the TFT_eSPI library.
- In the ~.pio\libdeps\lilygo-t-display-s3\TFT_eSPI\User_Setup_Select.h file, make sure to:
  - comment out line 27 (#include <User_Setup.h>) and,
  - uncomment line 133 (#include <User_Setups/Setup206_LilyGo_T_Display_S3.h>)
- Only once the User_Setup_Select.h has been modified should the code be uploaded to the T-Display-S3.
