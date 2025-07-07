# EBTN

An ESP-IDF component library for buttons and rotary encoders. 

Uses queues for button and encoder events. Supports both long button press and multiple button clicks (e.g. double, triple, or more clicks). 
Provides time deltas for most events (e.g. button click duration, time since last press, etc) and supports custom state polling for when you need to use port expanders (such as the [PCF8574](https://github.com/saawsm/pcf8574)).

## Usage 

```batch
cd components
git submodule add https://github.com/saawsm/ebtn
```

Add `ebtn` in project `CMakeLists.txt`