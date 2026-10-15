# Kinect v1 + Nuitrack SDK — Setup Guide

## Prerequisites
- Kinect v1 (Xbox 360) sensor
- Kinect USB/Power adapter (required — the sensor won't power via standard USB)
- macOS 11+ (tested on macOS 13+)
- ~2GB free disk space

## 1. Install Nuitrack SDK

1. Download Nuitrack from: https://nuitrack.com/
   - Click "Download" → select "macOS"
   - Or direct: https://download.3divi.com/Nuitrack/v0.38.5/nuitrack-macos-x64.zip
   
2. Extract to your home directory:
   ```bash
   cd ~
   unzip ~/Downloads/nuitrack-macos-x64.zip
   ```
   This creates `~/nuitrack/` with `include/`, `lib/`, `bin/` folders.

3. Verify installation:
   ```bash
   ls ~/nuitrack/include/nuitrack/Nuitrack.h  # Should exist
   ls ~/nuitrack/lib/libnuitrack.dylib        # Should exist
   ```

## 2. Activate License (FREE for non-commercial use)

Nuitrack offers a FREE non-commercial license for personal/artistic/educational projects.

```bash
cd ~/nuitrack
./nuitrack_license_tool activate
```

Follow the browser prompt:
1. Sign up with your email
2. Select "Non-Commercial License" (FREE)
3. Copy the activation key back to the terminal

Alternative: Use the 3-minute trial mode (no registration needed).
Just skip activation — the sensor works for 3 minutes per session.

## 3. Connect Kinect v1

1. Plug the USB/Power adapter into a wall outlet
2. Connect the Kinect sensor cable to the adapter
3. Connect the USB cable from the adapter to your Mac
4. The Kinect fan should spin up and the LED should turn green

Verify the device is visible:
```bash
system_profiler SPUSBDataType | grep -A 5 "Kinect"
```

## 4. Configure the Project

Edit `config.make` in the project root. Uncomment the Nuitrack section:

```make
# Nuitrack SDK — uncomment these lines:
PROJECT_CFLAGS = -I$(HOME)/nuitrack/include -DHAS_NUITRACK
PROJECT_LDFLAGS = -L$(HOME)/nuitrack/lib -lnuitrack -Wl,-rpath,$(HOME)/nuitrack/lib
```

This enables:
- `-DHAS_NUITRACK` → activates the real adapter code
- Include path → finds Nuitrack headers
- Library path → links libnuitrack.dylib
- RPATH → finds the library at runtime

## 5. Switch to Real Adapter

In `src/ofApp.cpp`, change the Kinect adapter from Mock to Nuitrack:

```cpp
// Find this line (~line 121):
kinectAdapter_ = std::make_unique<modular::MockKinectAdapter>(30.0f);

// Replace with:
kinectAdapter_ = std::make_unique<modular::NuitrackKinectAdapter>();
```

## 6. Build and Run

```bash
make -j4
make RunRelease
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| "No device found" | Check Kinect USB/power adapter is plugged in. The sensor needs external power. |
| "License expired" | Re-activate or use trial mode. Non-commercial license is permanent. |
| Link error: -lnuitrack not found | Verify paths in config.make. Check `~/nuitrack/lib/libnuitrack.dylib` exists. |
| "dyld: Library not loaded" | The RPATH is not set. Run: `install_name_tool -add_rpath ~/nuitrack/lib bin/ProyectoDanza2026` |
| Skeleton tracking slow | Reduce frame rate in Nuitrack config. Edit `~/nuitrack/data/nuitrack.config`. |
| App crashes on startup | Run with Nuitrack debug: `export NUITRACK_LOG_LEVEL=trace && make RunRelease` |

## Optional: Test Nuitrack Standalone

Before integrating, verify Nuitrack works independently:

```bash
cd ~/nuitrack
./bin/nuitrack_sample
```

You should see skeleton tracking output in the terminal.

## Kinect v1 Hardware Notes

- The Kinect requires a special USB/power adapter (~$10 on Amazon)
- USB port alone does NOT provide enough power
- The adapter combines USB data + 12V power into the proprietary Kinect connector
- Search: "Kinect Xbox 360 AC Adapter Power Supply"
