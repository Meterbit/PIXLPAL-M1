#ifndef EXAMPLE_DESIGN_UI_COMP_H
#define EXAMPLE_DESIGN_UI_COMP_H

#include <Arduino.h>

static const char* manifest = R"json(
{
  "appID": "11/0",
  "name": "UI App Practice",
  "author": "Stephen G.",
  "version": "v0.0.2"
}
)json";

static const char* api = R"json(
{
  "commands": {
    "SET_COLOR":   { "app_command": 0, "args": ["name","value"] },
    "REQUEST_NTP": { "app_command": 1 },
    "PRINCE_CHARM": { "app_command": 2 }
  },
  "successReply": { "jsonKey": "response", "successValue": 1 }
}

)json";

static const char* ui = R"json(
{
  "title": "Dev Practice App",
  "about": "Calendar Clock shows the current time/date. Customize the clock by selecting your preferred colors.",
  "widgets": [
    { "type": "button", "label": "TEST BUTTON", "command": "PRINCE_CHARM" },
    { "type": "button", "label": "TEST BUTTON", "command": "PRINCE_CHARM" },
    { "type": "button", "label": "TEST BUTTON", "command": "PRINCE_CHARM" }
  ],
  "toasts": [
    { "when": { "pxp_command": 0, "response": 1 }, "text": "Saved" },
    { "when": { "pxp_command": 1, "response": 1 }, "text": "NTP Time Requested" },
    { "when": { "pxp_command": 253 }, "text": "App launch successful" },
    { "when": { "pxp_command": 254 }, "text": "Launch the app and try again" },
    { "when": { "pxp_command": 255 }, "text": "App is already active" }
  ]
}

)json";

#endif


