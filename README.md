# th06-3ds-native

This project was made possible by the amazing work that has been put into the Touhou 6 decomp projection https://github.com/GensokyoClub/th06. I simply ported the game to the 3ds, they did all the work of decompiling th06.

# Installing

Obtain the following files/folders from a legally obtained copy of Touhou 6 (v1.02) into a folder `th06` on the root of your 3ds sdcard.
- `bgm/`
- `紅魔郷CM.DAT`
- `紅魔郷ED.DAT`
- `紅魔郷IN.DAT`
- `紅魔郷MD.DAT`
- `紅魔郷ST.DAT`
- `紅魔郷TL.DAT`
- `msgothic.ttc`

Download and install the CIA/3dsx from the releases section or scan this QR code in FBI:

![FBI install QR code](assets/v1.0.3-beta.png)

# Features

- Switch between 5:3 and 4:3 aspect ratio using display mode in options.
- Antialiasing: Pressing select will switch between the 3 antialiasing modes
    - Mode 0: No antialiasing (Default)
    - Mode 1: 2x antialiasing (Worst performance)
    - Mode 2: 1.5x antialiasing (A good balance of visuals and performance)

# Building

### Dependencies
- `SDL2`
- `SDL2_image`
- `SDL2_ttf`

Clone the SDL repositories

`git clone https://github.com/libsdl-org/SDL.git -b SDL2`

`git clone https://github.com/libsdl-org/SDL_image.git -b SDL2`

`git clone https://github.com/libsdl-org/SDL_ttf.git -b SDL2`

Run the following in each repository
```
cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/3DS.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

Clone this repository and run
```
make
```
# TODO
- Implement linear fog rendering
- Stereoscopic 3d rendering 


# Credits

Code is based on the portable branch of https://github.com/GensokyoClub/th06

Zun for creating Touhou
