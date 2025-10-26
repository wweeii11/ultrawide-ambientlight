# ultrawide-ambientlight

---

A utility program that renders ambient light effects in the black bar areas when playing a game or video content.

Reduces the risk of uneven display wear or burn-in on OLED displays.

Supports any borderless fullscreen game and non-DRM videos in a media player or browser.

Scenarios:
- Fixed 16:9 aspect game on a 21:9 display
- 21:9 content on a 32:9 display
- Ultrawide content on a regular 16:9 display

## Usage

1. Launch `ambientlight.exe` and choose auto-detection or manual resolution configuration.
2. Start your game in borderless fullscreen, or play video content in fullscreen in a media player or browser.
3. Use the configuration UI to adjust the effects to your liking.
4. Leave the program running in the background when using auto-detection mode.

## Screenshot

![screenshot](images/screen1.png)
![screenshot](images/screen2.png)
![screenshot](images/screen3.png)
![screenshot](images/screen4.png)

## Configurations

- `Resolution`: Auto-detection is recommended. For manual mode you can enter a resolution (e.g. `1920x1080`) or an aspect ratio (e.g. `16:9`).
- `Blur`: Adjust blur intensity to taste.
- `Vignette`: Allow semi-transparency in the corners so overlays (e.g. FPS counters) remain visible.
- `Mirror`: Apply a horizontal mirror to the effects to simulate a reflecting surface.
- `Frame rate`: Rendering frame rate for the effects.

## Third-party Libraries
- [inipp](https://github.com/mcmtroffaes/inipp)
- [DirectXTK](https://github.com/microsoft/DirectXTK)
- [imgui](https://github.com/ocornut/imgui)
