# ultrawide-ambientlight

---

This is a simple program for extending your game image into black bars (letterbox) when playing a game that doesn't support native ultrawide resolutions (e.g. game with fixed 16:9 aspect on a 21:9 display, or 21:9 game on a 32:9 display, or even ultrawide content on a regular 16:9 display).

Reduce risk of uneven display wear or burn-in for OLED users.

## Usage

- Start your game in borderless fullscreen game, and run ambientlight.exe
- Press ESC key to show/hide the settings window
- To exit, close the ambientlight window from your taskbar
- By default it captures an 16:9 box area of the desktop and renders in the area outside of this box

## Screenshot

![screenshot](images/screen1.png)
![screenshot](images/screen2.png)
![screenshot](images/screen3.png)
![screenshot](images/screen4.png)

## Config.ini

- Resolution: Select one of the those values based on the game or content that you will be playing (FHD, QHD, WFHD, WQHD, or custom), when using custom, enter the width and height in the Custom section. For example, if you are playing a game at 1920x1080 resolution on a 2560x1080 display, set FHD.
- BlurStrength: The number of blur passes that are applied
- BlurDownscale: The size of downsacle for blur, higher value will give a more detailed effect
- Mirror: Apply a horizontal mirroring to the effects, simulating a reflecting surface
- FrameRate: rendering frame rate of the effects

## Credits
[inipp](https://github.com/mcmtroffaes/inipp) <br />
[DirectXTK](https://github.com/microsoft/DirectXTK) <br />
[imgui](https://github.com/ocornut/imgui) <br />
