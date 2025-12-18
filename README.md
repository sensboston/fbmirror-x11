# fbmirror-x11

A lightweight X11 screen mirroring utility for SPI displays on Raspberry Pi.

Efficiently mirrors the X11 display to a secondary framebuffer (e.g., SPI LCD) with cursor support. Designed as a low-CPU alternative to ffmpeg-based solutions.

## Photo
<img src="https://github.com/user-attachments/assets/c2a6000c-e427-456c-9494-0f46ba20df7a" width="50%">


## Performance

| Method | CPU Usage |
|--------|-----------|
| ffmpeg x11grab | ~43% |
| fbmirror-x11 | ~0.9% |

Tested on Raspberry Pi Zero 2 W with 3.5" ILI9486 SPI display (480x320).

## Requirements

- Raspberry Pi with 64-bit OS (Bookworm/Trixie)
- SPI display with fbtft driver (creates /dev/fb1)
- X11 with XShm and XFixes extensions

### Dependencies
```bash
sudo apt install build-essential libx11-dev libxext-dev libxfixes-dev
```

## Building
```bash
git clone https://github.com/YOUR_USERNAME/fbmirror-x11.git
cd fbmirror-x11
make
sudo make install
```

## Usage
```bash
DISPLAY=:0 fbmirror_x11 [fps]
```

- `fps` - frames per second (default: 30)

Example:
```bash
DISPLAY=:0 fbmirror_x11 30
```

## Autostart Setup

### 1. Create startup script
```bash
sudo tee /usr/local/bin/mirror-display.sh << 'SCRIPT'
#!/bin/bash
export DISPLAY=:0

sleep 5

# Set resolution to match SPI display
xrandr --newmode "480x320_60" 6.0 480 490 530 560 320 325 330 340 -hsync -vsync 2>/dev/null
xrandr --addmode HDMI-1 "480x320_60" 2>/dev/null
xrandr --output HDMI-1 --mode "480x320_60"

exec /usr/local/bin/fbmirror_x11 30
SCRIPT
sudo chmod +x /usr/local/bin/mirror-display.sh
```

### 2. Create autostart entry
```bash
mkdir -p ~/.config/autostart
cat > ~/.config/autostart/mirror-display.desktop << 'DESKTOP'
[Desktop Entry]
Type=Application
Name=Mirror Display
Exec=/usr/local/bin/mirror-display.sh
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
DESKTOP
```

## Raspberry Pi Configuration

### /boot/firmware/config.txt

Required settings for dual framebuffer with SPI display:
```ini
dtoverlay=vc4-fkms-v3d
max_framebuffers=2

dtparam=spi=on
dtoverlay=tft35a:rotate=90

framebuffer_width=480
framebuffer_height=320
hdmi_force_hotplug=1
hdmi_group=2
hdmi_mode=87
hdmi_cvt=480 320 60 6 0 0 0
```

Note: Use `vc4-fkms-v3d` (fake KMS), not `vc4-kms-v3d` (full KMS).

## Touchscreen Calibration

If your SPI display has a touchscreen (e.g., ADS7846):
```bash
sudo apt install xinput-calibrator
DISPLAY=:0 xinput_calibrator
```

Save the output to `/etc/X11/xorg.conf.d/99-calibration.conf`.

## How It Works

1. Captures X11 screen using XShm (shared memory) for efficiency
2. Retrieves cursor image using XFixes extension
3. Converts 32-bit BGRA to 16-bit RGB565 format
4. Writes directly to /dev/fb1 via memory mapping
5. Draws cursor with alpha blending only in cursor area (optimized)

## Why Not Use Existing Tools?

- **fbcp** / **fbcp-ili9341** - Require DispmanX, unavailable on 64-bit Raspberry Pi OS
- **ffmpeg x11grab** - Works but consumes ~43% CPU
- **raspi2fb** - Also requires DispmanX

This tool works on modern 64-bit Raspberry Pi OS without VideoCore libraries.

## Troubleshooting

### No display output
- Check that `/dev/fb1` exists: `ls -la /dev/fb*`
- Verify fbtft driver loaded: `lsmod | grep fbtft`

### Cursor not visible
- Ensure XFixes is available: `xdpyinfo | grep XFIXES`

### High CPU usage
- Reduce FPS: `fbmirror_x11 20`

## License

MIT License

## Contributing

Pull requests welcome!
