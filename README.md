# Sakura 

A high-performance minimal (~1k lines of code) terminal-based multimedia library that renders images, GIFs, and videos using SIXEL graphics with synchronized audio playback.

## Table of Contents

- [Features](#features)
- [Installation](#installation)
- [Usage](#usage)
 - [Technical Implementation](#technical-implementation)
- [Performance Optimizations](#performance-optimizations)
- [SIXEL Terminal Support](#sixel-terminal-support)
- [API Documentation](#api-documentation)
- [Examples](#examples)
- [Troubleshooting](#troubleshooting)

## Features

### Core Capabilities
- **SIXEL Graphics Rendering**: Pixel-perfect graphics directly in the terminal using libsixel
- **Multi-format Support**: Images (JPG, PNG, BMP), animated GIFs, and videos (MP4, AVI, MOV, MKV)
- **Synchronized Audio**: Real-time audio playback with video using ffplay
 - **Adaptive Scaling / Fit Modes**: Fill the terminal using fit modes (contain, cover, stretch)
- **URL Download**: Direct streaming from web URLs
- **Performance Optimization**: Predecode queue, target-FPS downsampling, adaptive palette/scale, and precise frame pacing

### Rendering Modes
1. **SIXEL Mode**: High-quality graphics with full color palette
2. **ASCII Color**: Block-based color rendering
3. **ASCII Grayscale**: Character-based monochrome rendering
4. **Exact Mode**: True-color terminal rendering with Unicode blocks

## Installation

### Build From Source

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install libopencv-dev libsixel-dev ffmpeg cmake build-essential

# Arch Linux
sudo pacman -S opencv sixel cpr ffmpeg cmake base-devel

# macOS (Homebrew)
brew install opencv libsixel cpr ffmpeg cmake
```

> [!NOTE]
> For Ubuntu/Debian users: `cpr` is not available as a package in Ubuntu/Debian repositories.  
You must clone and build it manually.

```bash
git clone https://github.com/libcpr/cpr.git
cd cpr
mkdir build && cd build
cmake ..
make
sudo make install
```

Alternatively, clone `cpr` into your project root and include it as a subdirectory in your `CMakeLists.txt`

#### Build Instructions

```bash
git clone https://github.com/Sarthak2143/sakura.git
cd sakura
mkdir build && cd build
cmake ..
make -j$(nproc)
```

#### Quick Start

```bash
./sakura
```

### NixOS

#### Flakes (recommended)
install module via flakes
```nix
{
  inputs.sakura.url = "github:sarthak2143/sakura";
  inputs.sakura.inputs.nixpkgs.follows = "nixpkgs";

  outputs = { self, nixpkgs, sakura }: {
    # change `yourhostname` to your actual hostname
    nixosConfigurations.yourhostname = nixpkgs.lib.nixosSystem {
      # customize to your system
      system = "x86_64-linux";
      modules = [
        ./configuration.nix
        sakura.nixosModules.sakura
      ];
    };
  };
}

# enable it in your configuration
programs.sakura.enable = true;
```

#### Home Manager 
install home-manager modules via flakes
```nix
{
  inputs.sakura.url = "github:sarthak2143/sakura";
  inputs.sakura.inputs.nixpkgs.follows = "nixpkgs";

  outputs = { self, nixpkgs, home-manager, sakura }: {
    homeConfigurations."username" = home-manager.lib.homeManagerConfiguration {
      # ...
      modules = [
        sakura.homeModules.sakura
        # ...
      ];
    };
  };
}

# enable it in home manager config
programs.sakura.enable = true;
```

#### Install CLI via flakes
you can run sakura ad-hoc without installing it.
```bash
nix run github:sarthak2143/sakura
```

you can also install it into NixOS modules
```nix
{
  inputs.sakura.url = "github:sarthak2143/sakura";
  inputs.sakura.inputs.nixpkgs.follows = "nixpkgs";

  outputs = { self, nixpkgs, sakura }: {
    # change `yourhostname` to your actual hostname
    nixosConfigurations.yourhostname = nixpkgs.lib.nixosSystem {
      # customize to your system
      system = "x86_64-linux";
      modules = [
        ./configuration.nix
        {
          environment.systemPackages = [ sakura.packages.${system}.default ];
        }
      ];
    };
  };
}
```

## Usage

### Interactive Menu

When you run `./sakura`, you'll see an interactive menu:

```
Sakura Video Player with SIXEL
1. Image
2. GIF
3. Video (URL)
4. Video (File)
Choose option (1-4):
```

### Command Examples

#### 1. Image Rendering
```bash
# Local image
./sakura
> 1
> /path/to/image.jpg

# Online image
./sakura
> 1  
> https://example.com/image.png
```

#### 2. GIF Animation
```bash
# Local GIF
./sakura
> 2
> /path/to/animation.gif

# Online GIF
./sakura
> 2
> https://example.com/animation.gif
```

#### 3. Video from URL
```bash
./sakura
> 3
> https://example.com/video.mp4
```

#### 4. Local Video File
```bash
./sakura
> 4
> /path/to/video.mp4
```

### Sample Media Files

Test with these sample commands:

```bash
# Test image rendering
echo -e "1\nhttps://picsum.photos/800/600" | ./sakura

# Test GIF animation  
echo -e "2\nhttps://media.giphy.com/media/3o7qE1YN7aBOFPRw8E/giphy.gif" | ./sakura

# Test local video
echo -e "4\nmedia/your_video.mp4" | ./sakura
```

## Technical Implementation

### SIXEL Graphics Pipeline

1. **Image/Video Decode**: OpenCV handles decoding
2. **Pre-scaling**: Reader thread pre-scales frames (INTER_NEAREST when `fastResize=true`, otherwise INTER_AREA)
   - Optional target-FPS downsampling to stabilize throughput
3. **SIXEL Encoding**: libsixel converts RGB data to SIXEL format
4. **Terminal Output**: Direct SIXEL sequence transmission with precise pacing

```cpp
// Core rendering pipeline
cv::Mat rgb_img;
cv::cvtColor(img, rgb_img, cv::COLOR_BGR2RGB);
sixel_dither_t *dither = sixel_dither_new(palette_size);
sixel_encode(rgb_img.data, width, height, 3, dither, output);
```

### Audio-Video Synchronization

The player implements sophisticated timing control:

```cpp
// High-precision frame timing
auto frame_duration_ns = std::chrono::nanoseconds(static_cast<long long>(1000000000.0 / fps));
auto target_frame_time = start_time + (frame_duration_ns * frame_number);

// Smart frame skipping
if (frame_number < target_frame_number) {
    int frames_behind = target_frame_number - frame_number;
    if (frames_behind > 2) {
        // Skip frames to catch up
    }
}
```

### Video Processing Architecture

```cpp
// Video processing flow
// Reader thread (pre-decode + pre-scale into a bounded queue)
// Main thread: encode to SIXEL and pace using steady_clock
```

## Performance Optimizations

### Frame Timing Optimizations

- **Predecode Queue**: Background thread decodes and scales frames into a bounded queue
- **Target FPS Downsampling**: Time-based input frame skipping to match a stable render rate
- **Adaptive Frame Skipping**: Drops multiple stale frames at once when far behind
- **Steady Clock Pacing**: `std::chrono::steady_clock` with sleep-until pacing
- **Buffered I/O**: Unit-buffered output plus explicit flush to avoid terminal buffering stalls
- **Memory Pre-allocation**: Reserved string buffers to avoid reallocations

### Video Quality / Throughput Settings

```cpp
// Adaptive scaling based on FPS
// Fit modes:
// - CONTAIN: keep aspect within terminal bounds (no crop)
// - COVER: fill entire terminal (may crop)
// - STRETCH: fill width and height (distorts aspect)

// Fast pre-scaling:
// options.fastResize = true; // use INTER_NEAREST for maximum FPS
```

### SIXEL Optimization

- **Palette Size**: Configurable color palette (typically 256)
- **Static Palette (optional)**: Reuse first-frame palette for more stable colors and less overhead
- **Adaptive Palette (optional)**: Shrink palette when behind, restore when caught up
- **Interpolation**: INTER_NEAREST for speed (when `fastResize=true`), INTER_AREA for quality

## SIXEL Terminal Support

### Compatible Terminals

| Terminal | SIXEL Support | Command |
|----------|---------------|---------|
| **xterm** | Native | `xterm -ti vt340` |
| **mlterm** | Native | Default |
| **wezterm** | Configurable | Enable in config |
| **foot** | Native | Default |
| **mintty** | Optional | `--enable-sixel` |
| **iTerm2** | Beta | Enable in preferences |

### Terminal Configuration

#### xterm Setup
```bash
# Launch with SIXEL support
xterm -ti vt340 -geometry 120x40

# Or add to ~/.Xresources
xterm*decTerminalID: vt340
```

#### wezterm Configuration
```lua
-- ~/.config/wezterm/wezterm.lua
return {
  enable_sixel = true,
  max_fps = 60,
}
```

## API Documentation

### Core Classes

#### `Sakura` Class

Main rendering engine with the following public methods:

```cpp
class Sakura {
public:
    // Image rendering
    bool renderFromUrl(const std::string &url, const RenderOptions &options);
    bool renderFromMat(const cv::Mat &img, const RenderOptions &options);
    
    // Video/GIF rendering  
    bool renderVideoFromFile(const std::string &path, const RenderOptions &options);
    bool renderVideoFromUrl(const std::string &url, const RenderOptions &options);
    bool renderGifFromUrl(const std::string &url, const RenderOptions &options);
    
    // SIXEL rendering
    std::string renderSixel(const cv::Mat &img, int paletteSize = 256);
};
```

#### `RenderOptions` Structure

```cpp
struct RenderOptions {
    int width = 0;
    int height = 0;
    int paletteSize = 256;
    CharStyle style = SIMPLE;
    RenderMode mode = EXACT;
    DitherMode dither = NONE;
    bool aspectRatio = true;
    double contrast = 1.2;
    double brightness = 0.0;
    double terminalAspectRatio = 1.0;
    int queueSize = 16;          // size of predecode queue
    int prebufferFrames = 4;     // frames to prebuffer before audio
    bool staticPalette = false;  // reuse first palette for all frames
    FitMode fit = COVER;         // STRETCH, COVER, CONTAIN
    bool fastResize = false;     // use INTER_NEAREST when true
    // Throughput controls
    double targetFps = 0.0;      // 0 = follow source FPS; otherwise downsample to this
    bool adaptivePalette = false;
    int minPaletteSize = 64;
    int maxPaletteSize = 256;
    bool adaptiveScale = false;  // dynamically adjust scale based on drop rate
    double minScaleFactor = 0.80;
    double maxScaleFactor = 1.00;
    double scaleStep = 0.05;
};
```

### Rendering Modes

```cpp
enum RenderMode {
    EXACT,
    ASCII_COLOR,
    ASCII_GRAY,
    SIXEL
};

enum FitMode {
    STRETCH,
    COVER,
    CONTAIN
};
```

## Examples

### Programmatic Usage

```cpp
#include "sakura.hpp"

int main() {
    Sakura renderer;
    RenderOptions options;
    options.mode = SIXEL;
    options.paletteSize = 256;
    options.width = 800;
    options.height = 600;
    options.queueSize = 48;
    options.prebufferFrames = 12;
    options.staticPalette = true;
    options.fit = COVER;
    options.fastResize = true;    // maximize FPS
    options.targetFps = 30.0;     // stabilize rendering on SIXEL terminals
    options.adaptivePalette = true;
    options.minPaletteSize = 64;
    options.maxPaletteSize = 256;
    options.adaptiveScale = true; // auto-tune size when drops persist
    options.minScaleFactor = 0.85;
    options.maxScaleFactor = 1.00;
    options.scaleStep = 0.05;
    
    // Render image
    renderer.renderFromUrl("https://example.com/image.jpg", options);
    
    // Render video with audio
    renderer.renderVideoFromFile("video.mp4", options);
    
    return 0;
}
```

### Custom Image Processing

```cpp
// Load and process image
cv::Mat image = cv::imread("input.jpg");
cv::GaussianBlur(image, image, cv::Size(5, 5), 0);

// Render with custom options
RenderOptions opts;
opts.contrast = 1.2;
opts.brightness = 10;
renderer.renderFromMat(image, opts);
```

### Batch Processing

```cpp
// Process multiple images
std::vector<std::string> urls = {
    "https://example.com/img1.jpg",
    "https://example.com/img2.jpg",
    "https://example.com/img3.jpg"
};

for (const auto& url : urls) {
    renderer.renderFromUrl(url, options);
    std::this_thread::sleep_for(std::chrono::seconds(2));
}
```

## TODO

- [ ] Reduce frame drops to <=5%
- [ ] Create error exception classes

## Troubleshooting

### Common Issues

#### "Failed to open video"
```bash
# Check file exists and permissions
ls -la /path/to/video.mp4

# Verify OpenCV codec support
ffmpeg -codecs | grep h264

# Try different format
ffmpeg -i input.mov -c:v libx264 -c:a aac output.mp4
```

#### No SIXEL output
```bash
# Test terminal SIXEL support
echo -e '\ePq"1;1;100;100#0;2;0;0;0#1;2;100;100;0#1~~@@vv@@~~@@~~$#0~~@@~~@@~~@@vv$#1!14~\e\\'

# Launch with SIXEL-capable terminal
xterm -ti vt340 -e ./sakura
```

#### Audio/video sync issues
```bash
# Check ffplay installation
which ffplay

# Test audio playback separately
ffplay -nodisp -autoexit video.mp4

# Check audio permissions (containers)
pulseaudio --check
```

#### Performance issues
```bash
# Reduce video resolution
ffmpeg -i input.mp4 -vf scale=640:480 output.mp4

# Lower frame rate
ffmpeg -i input.mp4 -r 15 output.mp4

# Use hardware acceleration if available
ffmpeg -hwaccel auto -i input.mp4 output.mp4
```

### Debug Mode

```cpp
// Enable debug output
#define SAKURA_DEBUG 1

// Check rendering statistics
std::cout << "Frames rendered: " << frames_rendered << std::endl;
std::cout << "Frames dropped: " << frames_dropped << std::endl;
std::cout << "Drop rate: " << (frames_dropped * 100.0 / total_frames) << "%" << std::endl;
```

### Log Analysis

Monitor performance metrics during playback:

```bash
# Terminal output shows real-time stats
Frame: 450/1800 Dropped: 23 Audio: ON

# Calculate performance
Drop Rate: 23/450 = 5.1%  # Acceptable: <10%
Render Rate: 94.9%         # Good: >90%
```

## Contributing

### Development Setup

```bash
git clone https://github.com/Sarthak2143/sakura.git
cd sakura

# Install development dependencies
sudo apt install clang-format cppcheck valgrind

# Build with debug symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```
### Testing

```bash
# Run basic functionality tests
./test_suite.sh

# Memory leak detection  
valgrind --leak-check=full ./sakura

# Performance profiling
perf record ./sakura
perf report
```

## Acknowledgments

- **libsixel** - SIXEL graphics encoding library
- **OpenCV** - Computer vision and image processing
- **FFmpeg** - Multimedia framework for audio/video
- **cpr** - C++ HTTP request library

---
