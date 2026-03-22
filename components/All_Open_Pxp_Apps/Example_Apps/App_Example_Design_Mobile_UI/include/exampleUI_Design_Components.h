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
    "type": "colorList",
    "title": "Clock/Date Colors",
    "items": [
      { "id": "hour_minute", "label": "Hour/Minute", "default": "0xFFFF6600" },
      { "id": "seconds", "label": "Seconds", "default": "0xFF006600" },
      { "id": "meridiem", "label": "Meridiem", "default": "0xFFFF66FF" },
      { "id": "weekday", "label": "Weekday", "default": "0xFFFF0000" },
      { "id": "date", "label": "Date", "default": "0xFF000000" }
    ],
    "onChange": {
      "command": { "app_command": 0, "args": ["name", "value"] },
      "args": {
        "id": "{item.id}",
        "value": "{color.hexAARRGGBB}"
      }
    }
  }
  ],
  "toasts": [
    { "when": { "app_command": 0 }, "text": "Saved" }
  ]
}
)json";



#endif



#ifndef EXAMPLE_DESIGN_UI_COMP_H
#define EXAMPLE_DESIGN_UI_COMP_H

static const char* ui_api = R"json(
{
  "widgets": [

    {
      "type": "colorList",
      "title": "Clock/Date Colors",
      "items": [
        { "id": "hour_minute", "label": "Hour/Minute", "default": "0xFFFF6600" },
        { "id": "seconds", "label": "Seconds", "default": "0xFF006600" },
        { "id": "meridiem", "label": "Meridiem", "default": "0xFFFF66FF" },
        { "id": "weekday", "label": "Weekday", "default": "0xFFFF0000" },
        { "id": "date", "label": "Date", "default": "0xFF000000" }
      ],
      "onChange": {
        "command": { "app_command": 0, "args": ["name", "value"] },
        "args": {
          "name": "{item.label}",
          "value": "{color.hexAARRGGBB}"
        }
      }
    },
    { "type": "borderButton", "label": "TEST BUTTON", "command": { "app_command": 1 } },
    {
      "type": "textColorField",
      "title": "Clock Title",
      "defaultText": "HAPPY HOME",
      "defaultColor": "0xFF886600",
      "command": { "app_command": 2, "args": ["clockTitle", "color"] }
    },

    {
      "type": "switch",
      "title": "Cycle Selected Stocks",
      "default": false,
      "command": {
        "app_command": 2,
        "args": ["cycleStocks"]
      }
    },

    {
      "type": "slider",
      "title": "Stock Change Interval",
      "min": 15,
      "max": 120,
      "default": 30,
      "suffix": "Secs",
      "command": {
        "app_command": 2,
        "args": ["dInterval"]
      }
    },

    {
      "type": "textField",
      "title": "Set API Key",
      "obscureText": false,
      "emptyHint": "Copy and paste API key here",
      "filledHint": "Change API key",
      "command": {
        "app_command": 3,
        "args": ["api_key"]
      }
    },

    {
      "type": "radioGroup",
      "title": "Fixtures/Standings",
      "default": 0,
      "command": {
        "app_command": 3,
        "args": ["value"]
      },
      "options": [
        { "label": "Show Fixtures", "value": 0 },
        { "label": "Show Standings", "value": 1 }
      ]
    },

    {
      "type": "writeup",
      "title": "Spotify Now Playing",
      "body": "Instantly see what's playing on your Spotify app without reaching for your phone or tablet. Get artist name, track title, album art, and playback status at a glance.\n\nUpdates automatically as your music changes.\n\nThis service requires internet connection and is available for Spotify Premium accounts only."
    },

    {
      "type": "richTextBlock",
      "title": "Spotify Now Playing",
      "blocks": [
        {
          "type": "paragraph",
          "text": "Instantly see what's playing on your Spotify app without reaching for your phone or tablet."
        },
        {
          "type": "bullet",
          "text": "Artist name"
        },
        {
          "type": "bullet",
          "text": "Track title"
        },
        {
          "type": "bullet",
          "text": "Album art"
        },
        {
          "type": "bullet",
          "text": "Playback status"
        },
        {
          "type": "paragraph",
          "text": "Updates automatically as your music changes."
        },
        {
          "type": "note",
          "text": "This service requires internet connection and is available for Spotify Premium accounts only."
        }
      ]
    },

    {
      "type": "richTextBlock",
      "title": "Spotify Now Playing",
      "blocks": [
        {
          "type": "paragraph",
          "text": "Instantly see what's playing on your Spotify app without reaching for your phone or tablet."
        },
        {
          "type": "paragraph",
          "text": "Updates automatically as your music changes."
        },
        {
          "type": "note",
          "text": "This service requires internet connection and is available for Spotify Premium accounts only."
        }
      ]
    },

    {
      "type": "richTextBlock",
      "title": "Spotify Now Playing",
      "blocks": [
        {
          "type": "paragraph",
          "spans": [
            { "text": "Visit " },
            {
              "text": "Spotify",
              "link": "https://spotify.com"
            },
            { "text": " to learn more." }
          ]
        }
      ]
    },

    {
      "type": "dropDownTextField",
      "title": "Select Pattern",
      "dataset": "audio_patterns",
      "dropDownItemCount": 6,
      "defaultValue": 0,
      "command": {
        "app_command": 0,
        "args": ["selectedPattern"]
      }
    },
    {
      "type": "dropDownTextField",
      "title": "No of Bands",
      "dataset": "band_counts",
      "dropDownItemCount": 6,
      "defaultValue": 0,
      "command": {
        "app_command": 1,
        "args": ["numOfBands"]
      }
    },
    {
      "type": "dropDownList",
      "title": "Selected Stocks",
      "dataset": "stock_symbols",
      "buttonLabel": "Add/Remove Stocks",
      "itemTapCommand": {
        "app_command": 0,
        "args": ["stkSymbol"]
      },
      "command": {
        "app_command": 1,
        "args": ["stkList"]
      }
    }
    
  ],
  "toasts": [
    { "when": { "app_command": 0 }, "text": "Saved" },
    { "when": { "app_command": 1 }, "text": "NTP Time Requested" }
  ]
}
)json";


static const char* data = R"json(
    {
      "datasets": 
      {
        "audio_patterns": [
          { "name": "1. BoxedBars BluePeak 1", "value": 0 },
          { "name": "2. BoxedBars BluePeak 1", "value": 1 },
          { "name": "3. BoxedBars RedPeak", "value": 2 }
        ],
        "band_counts": [
          { "name": "8 Bands", "value": 0 },
          { "name": "16 Bands", "value": 1 },
          { "name": "24 Bands", "value": 2 },
          { "name": "32 Bands", "value": 3 },
          { "name": "64 Bands", "value": 4 }
        ],
        "stock_symbols": [
          { "name": "AAPL", "value": "AAPL" },
          { "name": "MSFT", "value": "MSFT" },
          { "name": "NVDA", "value": "NVDA" }
        ]
      }
    }
)json";

#endif


