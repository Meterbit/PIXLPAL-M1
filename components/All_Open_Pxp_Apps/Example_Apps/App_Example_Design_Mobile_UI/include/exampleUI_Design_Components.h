#ifndef EXAMPLE_DESIGN_UI_COMP_H
#define EXAMPLE_DESIGN_UI_COMP_H

#include <Arduino.h>

static const char* manifest = R"json(
{
  "appID": "11/0",
  "name": "UI App Practice",
  "about": "Write the description of your app here.",
  "author": "Stephen G.",
  "version": "v0.0.2"
}
)json";

static const char* ui_api = R"json(
{
  "widgets": [
    {
      "type": "button",
      "label": "TEST BUTTON",
      "command": { "app_command": 2 }
    },
    {
      "type": "colorList",
      "title": "Clock/Date Colors",
      "items": [
        { "label": "Hour/Minute", "valueKey": "Hour/Minute", "default": "0xFFFF6600" },
      ],
      "onChange": {
        "command": { "app_command": 0, "args": ["name", "value"] },
        "args": {
          "name": "{item.valueKey}",
          "value": "{color.hexAARRGGBB}"
        }
      }
    }
  ],
  "toasts": [
    { "when": { "pxp_command": 0, "response": 1 }, "text": "Saved" },
    { "when": { "pxp_command": 1, "response": 1 }, "text": "NTP Time Requested" }
  ],
  "successReply": { "jsonKey": "response", "successValue": 1 }
}
)json";


#endif


