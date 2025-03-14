# ultrawide-ambientlight

---

This is a simple program that renders a blurred image effect in the black bar areas of an ultra wide display.
Especially useful for 32:9 display owners, where game support is limited. Reduce risk of uneven display wear.

## Usage

- Start your game in borderless fullscreen game, and run ambientlight.exe
- To exit, close the ambientlight window from your taskbar
- By default it captures an 16:9 box area of the desktop and renders in the area outside of this box
- Edit the config.ini to apply different resolution and effects

## Screenshot

![screenshot](images/screen1.png)
![screenshot](images/screen2.png)
![screenshot](images/screen3.png)

## Config.ini

- Resolution: Select one of the those values based on the game or content that you will be playing (FHD, QHD, WFHD, WQHD, or custom), when using custom, enter the width and height in the Custom section
- BlurStrength: The number of blur passes that are applied
- BlurDownscale: The size of downsacle for blur, higher value will give a more detailed effect
- Mirror: Apply a horizontal mirroring to the effects, simulating a reflecting surface
- UpdateInterval: Vsync interval of the effect rendering, range 1 to 4

## Credits
[inipp](https://github.com/mcmtroffaes/inipp) <br />
[DirectXTK](https://github.com/microsoft/DirectXTK) <br />
