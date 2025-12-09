# Godroll Launcher

A sleek, modern desktop launcher for quick Destiny 2 weapon search on [Godroll.tv](https://godroll.tv). Built with Qt 6 and QML for a beautiful, responsive experience.

![Platform](https://img.shields.io/badge/Platform-Windows-blue)
![Qt](https://img.shields.io/badge/Qt-6.10+-green)
![License](https://img.shields.io/badge/License-MIT-yellow)

## Features

### 🔍 Smart Search
- **Multi-field search** - Search by weapon name, weapon type (Pulse Rifle, Hand Cannon, etc.), frame type (Aggressive, Rapid-Fire, etc.), or season
- **Multi-term queries** - Type "pulse micro-missile" to find Pulse Rifles with Micro-Missile frame
- **Hyphen normalization** - "high impact" matches "High-Impact Frame"
- **Fuzzy matching** - Handles typos and partial matches
- **Visual match highlighting** - Gold badges show which metadata matched your search

### 🎨 Beautiful UI
- **Glassmorphism design** - Modern semi-transparent backdrop with blur effects
- **Weapon icons** - High-quality weapon thumbnails from Bungie API
- **Damage type icons** - Arc, Solar, Void, Stasis, Strand, Kinetic indicators
- **Ammo type icons** - Primary, Special, Heavy ammo indicators
- **Holofoil badge** - Animated rainbow gradient badge for holofoil weapons
- **Season indicators** - Season number and name display
- **Smooth animations** - Hover effects, color transitions, and list animations

### ⌨️ Keyboard-First Experience
- **Alt + G** - Global hotkey to toggle launcher (works system-wide)
- **↑↓ Arrow keys** - Navigate through search results
- **Enter** - Open selected weapon on godroll.tv
- **Middle-click** - Open weapon without closing launcher
- **ESC** - Close launcher or clear search
- **Auto-focus** - Search input is focused on open

### 🖥️ System Integration
- **System tray** - Lives in your system tray for quick access
- **Auto-start option** - Optional Windows startup integration
- **Minimal footprint** - Low memory and CPU usage
- **Single instance** - Prevents multiple launcher windows

### 📦 Data
- **Live API** - Fetches weapon data from godroll.tv API
- **Latest season filter** - Shows newest weapons by default
- **Comprehensive metadata** - Weapon type, frame, season, damage type, ammo type, holofoil status

## Installation

### Download Release
1. Go to [Releases](../../releases)
2. Download the latest `GodrollLauncher-vX.X.X.zip`
3. Extract to your preferred location
4. Run `GodrollLauncher.exe`

### Build from Source

#### Prerequisites
- Qt 6.10+ with MinGW
- CMake 3.16+

#### Build

```bash
cd app
mkdir build
cd build
cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.10.1/mingw_64 ..
cmake --build . --config Release
```

#### Run

```bash
./GodrollLauncher.exe
```

## Usage

| Shortcut | Action |
|----------|--------|
| `Alt + G` | Toggle launcher window (global hotkey) |
| `↑` / `↓` | Navigate results |
| `Enter` | Open weapon on godroll.tv |
| `Middle-click` | Open weapon (keep launcher open) |
| `ESC` | Close launcher / Clear search |

### Search Examples
- `ace` → Ace of Spades
- `pulse` → All Pulse Rifles
- `aggressive` → All Aggressive Frame weapons
- `season 25` → All Season 25 weapons
- `revenant` → Season of the Revenant weapons
- `pulse high-impact` → High-Impact Pulse Rifles
- `void sword` → Void damage Swords

## Tech Stack

- **Framework**: Qt 6.10 with QML
- **Language**: C++17
- **Build**: CMake + MinGW
- **API**: godroll.tv REST API
- **CI/CD**: GitHub Actions (auto-build, auto-version, auto-release)

## License

MIT License - See LICENSE file for details.

