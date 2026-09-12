# WeatherClock LVGL SDL Preview

This simulator previews the 400 x 300 LVGL clock UI on macOS with SDL2.

Build from the firmware project directory:

```sh
cd <project-root>/RLCD_CLOCK
cmake -S simulator -B simulator/build
cmake --build simulator/build -j4
```

Run:

```sh
cd <project-root>/RLCD_CLOCK
./simulator/build/weather_clock_sdl
```

The preview window is scaled to 800 x 600, while the LVGL canvas remains the
same 400 x 300 size as the device. Press `Esc` or close the window to quit.

Module ownership:

- `main.cpp`: preview mode selection, shared page chrome, time progression and event loop.
- `sdl_preview_backend.*`: SDL resources, LVGL flush and PPM screenshot output.
- `sdl_preview_widgets.*`: shared labels, monochrome bars and canvas drawing primitives.
- `sdl_preview_settings.*`: settings-page previews.
- `sdl_preview_calendar.*`: calendar grid and lunar preview body.
- `sdl_preview_flip_cards.*`: shared inverted DSEG cards used by the temperature/humidity clock and Xiaozhi previews.
- `sdl_preview_flip_clock.*`: temperature/humidity clock sensor, mood and date preview body.
- `sdl_preview_gallery.*`: gallery image, block-clock and daily-saying preview body.
- `sdl_preview_history.*`: temperature/humidity history chart body.
- `sdl_preview_weather.*`: weather-board body and shared QWeather icon conversion.
- `sdl_preview_xiaozhi.*`: Xiaozhi dialogue, preparing and Pomodoro preview body.
