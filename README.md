# Mina Practice Mod
A practice mode for speedrunning Mina the Hollower, for Windows and Linux.

## Features
### Save Management
For now, the only feature included is quick save backups and reloads, which can be performed at any point during the game.

There are 999 save slots available. You can switch between them, write a save to that slot, and reload it whenever you want.

These saves are stored in a `saves` directory (right where the mod is installed). By default they are named as `xxx.sv` (e.g. `001.sv`).

The mod relies on these saves starting with the 3-digit number corresponding to the save slot they're intended for. However, they can be given a custom name by appending a `.Name` after the slot number (e.g. `001.Bone Beach Start.sv`). This name will then be displayed in-game when switching to that save slot.

## Configurations
Some configurations are available through a `PracticeMod.cfg` file, which is generated the first time the mod is run. This file can be opened with any text editor (like Notepad). Any changes to this config file will need a full game reboot to be applied.

The config file looks something like this:

```
[General]
EnableLogging = false

[Keybinds]
SaveBind = R2,R3
LoadBind = R2,L3
SlotDownBind = R2,RSTICK_LEFT
SlotUpBind = R2,RSTICK_RIGHT
```

Each keybind entry is assigned a key combo, and whenever that combination is hit in-game, the corresponding action will be performed.

Each key combo is comma-separated.

The list of valid key options can be looked up [here](https://github.com/YachtClubGames/MinaModAPI/blob/728af6e5dc5ec9f8eba4a1b69c76e147fcea8dde/MinaModEnums.h#L6). All the YC_KEY_ and YC_INPUT_ entries are valid, though for this config file you will have to strip out those prefixes.

There is also an `EnableLogging` option, which will control whether some logs are written to `mod.log` or not. This is for debug purposes, so feel free to leave that on false for general use.

## Download
You can download this mod from the **[Releases Page](https://github.com/SBDWolf/MinaTheHollowerPracticeMod/releases)**.

## Installation
- On Steam, switch to the game's experimental-modding branch (right click on Mina the Hollower in your library, select Properties, then Game Versions & Beta)
- Still under the game's properties, switch to the General tab, then add `-mod -mod-allow-code` to the Launch Options.
- Download the Practice Mod [here](https://github.com/SBDWolf/MinaTheHollowerPracticeMod/releases) for your Operating System if you haven't already, and extract it to your mods folder.
  - On Windows, this is in `%AppData%\Yacht Club Games\Mina the Hollower\mods`.
  - On Linux, this is in `./.local/share/Yacht Club Games\Mina the Hollower\mods`.
  - In both cases, you should create the `mods` folder if it doesn't exist.
    - At the end of this, you should have a `mods` folder at save level of `saveData.yc`, and inside that `mods` folder, you should have a `PracticeMod` folder containing all the relevant files.
- Launch the game. The mod should hopefully be running!
- The first boot will generate the aforementioned `PracticeMod.cfg`. You can edit that file to change your keybinds.

## Building
If you're a regular user looking to simply acquire the practice mod to use it for youself, you do not need to do this! Head on over to the [releases page](https://github.com/SBDWolf/MinaTheHollowerPracticeMod/releases) and download from there. This section is just for those looking to develop themselves, or who want to build directly form the source code for other reasons.

There's probably a few different ways of building this, but what I do is:
### Windows
Open `CMakeLists.txt` with Visual Studio (you might need to udpate Visual Studio to a newer version in order to be able to open this file), then build (either from the Build dropdown > Build {Project Name}, or by hitting Ctrl+B). You can select the build profile to be `Release|x64` towards the middle-top.

There's also a post-build action that automatically moves the compiled `mod.dll` into `%AppData%\Yacht Club Games\Mina the Hollower\mods\PracticeMod`

### Linux
Open the project directory in a terminal and run `build_linux.sh`. `mod.so` will be placed inside the newly-created `build` directory.

## Packaging
Again, if you're a regular user, you don't need to do this, you can download the mod from the [releases page](https://github.com/SBDWolf/MinaTheHollowerPracticeMod/releases).

Once the .dll and .so have been built, you can package them up into zips ready for release using either `package\package.bat` on Windows, or `package/package.sh` on Linux. Either script will automatically take both the built `mod.dll` and `mod.so` and package them up into zips along with the other needed files.

You might need to install zip on Linux before running this script (`sudo apt install zip` on Debian-based distros).

