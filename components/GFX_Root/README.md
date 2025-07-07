The root/core graphics primitive functions extracted from the Adafruit GFX Library. Originally created by [Jean-Marc Zingg] (https://github.com/ZinggJM/GFX_Root).

# Change made by MrFaptastic
### Version 3.0.0
* Fixed: https://github.com/adafruit/Adafruit-GFX-Library/issues/327#issuecomment-762309664
* Implement FastLED CRGB color value (24bit) support for all graphics functions in addition to legacy 16-bit 565 colors.

### Version 2.0.0
- reduced virtual methods to preserve code space
- class GFX has no transaction support for minimal code space use
### Version 1.8.2
- extract from Adafruit GFX Library version 1.8.2
