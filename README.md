# Godroll Launcher

A desktop launcher for quick Destiny 2 weapon search on [Godroll.tv](https://godroll.tv). Built with Qt 6 and QML.

![Platform](https://img.shields.io/badge/Platform-Windows-blue)
![Qt](https://img.shields.io/badge/Qt-6.10+-green)
![License](https://img.shields.io/badge/License-MIT-yellow)

## Features

### Search
- Search by weapon name, weapon type, frame type, season, damage type, or ammo type
- Multi-term queries - Type "pulse high-impact void" to find High-Impact Void Pulse Rifles
- Fuzzy matching - Handles typos and partial matches
- Season search - Type "s28", "Season 28", or "Revenant" to filter by season

### Advanced Filters
- **`-h`** - Show only holofoil weapons (or use "holofoil"/"holo" keyword)
- **`-!`** - Show only one weapon per name (removes duplicates)
- **`-*`** - Remove 50-result limit, show all matches
- **`-a`** - Show only Adept/Harrowed/Timelost weapons (or use "adept" keyword)
- **Combined** - Use together like `-!*h` or `-h -! -*`

### Keyboard Shortcuts
- **`Alt + G`** - Toggle launcher (works globally)
- **`↑` / `↓`** - Navigate results
- **`Enter`** - Open selected weapon on godroll.tv
- **`Middle-click`** - Open weapon without closing launcher
- **`ESC`** - Close launcher or clear search
- **`F5`** - Reload weapon data

### Interface
- Weapon icons with damage type and ammo indicators
- Animated holofoil badge for holofoil weapons
- Season information with expansion names
- System tray integration
- Auto-start with Windows option

## Installation

### Download Release
1. Go to [Releases](../../releases)
2. Download the latest `GodrollLauncher-vX.X.X.zip`
3. Extract to your preferred location
4. Run `GodrollLauncher.exe`
5. (Optional) Right-click system tray icon to enable "Start with Windows"

### Build from Source

Requirements: Qt 6.10+ with MinGW, CMake 3.16+

```bash
mkdir build
cd build
cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.10.1/mingw_64 ..
cmake --build . --config Release
./GodrollLauncher.exe
```

## Usage

### Search Examples

**Basic Search**
```
ace                    → Ace of Spades
pulse                 → All Pulse Rifles
aggressive            → All Aggressive Frame weapons
void                  → All Void damage weapons
```

**Multi-Term Search**
```
pulse high-impact     → High-Impact Pulse Rifles
void sword            → Void damage Swords
hand cannon solar     → Solar Hand Cannons
```

**Season Search**
```
s28                   → Season 28 weapons
season 28             → Season 28 weapons
revenant              → Season of the Revenant weapons
```

**Advanced Filters**
```
-h                    → Holofoil weapons only
-h pulse              → Holofoil Pulse Rifles
-! s28                → Unique weapons from Season 28
-* pulse              → All Pulse Rifles (no limit)
-a                    → Adept only
-!*h                  → All unique holofoil weapons
```

### System Tray
- Left-click to show/hide launcher
- Right-click for options menu (auto-start, exit)

## Troubleshooting

**Launcher won't start**
- Check if another instance is already running (check system tray)
- Run from command line to see error messages

**Global hotkey not working**
- Check if another application is using `Alt+G`
- Restart the launcher after closing conflicting applications

**Weapons not loading**
- Check internet connection
- Press `F5` to reload

## Links

- Godroll.tv: https://godroll.tv
- Repository: https://github.com/bugrakaan/godroll.tv-app

## License

MIT License

---

Created with ♥ by [Diabolic#5311](https://www.bungie.net/7/en/User/Profile/3/4611686018520824383)

