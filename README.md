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
- **Adaptive Scaling**: Automatic sizing to terminal dimensions with aspect ratio preservation
- **URL Download**: Direct streaming from web URLs
- **Performance Optimization**: Advanced frame timing and skipping for smooth playback

### Rendering Modes
1. **SIXEL Mode**: High-quality graphics with full color palette
2. **ASCII Color**: Block-based color rendering
3. **ASCII Grayscale**: Character-based monochrome rendering
4. **Exact Mode**: True-color terminal rendering with Unicode blocks

## Installation

### Dependencies

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install libopencv-dev libsixel-dev libcpr-dev ffmpeg cmake build-essential

# Arch Linux
sudo pacman -S opencv sixel cpr ffmpeg cmake base-devel

# macOS (Homebrew)
brew install opencv libsixel cpr ffmpeg cmake
```

### Build Instructions

```bash
git clone https://github.com/Sarthak2143/sakura.git
cd sakura
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Quick Start

```bash
./sakura
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

1. **Image Processing**: OpenCV handles decoding and format conversion
2. **Color Quantization**: K-means clustering for optimal palette generation
3. **SIXEL Encoding**: libsixel converts RGB data to SIXEL format
4. **Terminal Output**: Direct SIXEL sequence transmission

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
auto frame_duration_ns = static_cast<long long>(1000000000.0 / fps);
auto target_frame_time = start_time + std::chrono::nanoseconds(frame_number * frame_duration_ns);

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
cv::VideoCapture cap(video_path);
while (cap.read(frame)) {
    cv::resize(frame, resized, target_size, 0, 0, cv::INTER_NEAREST);
    std::string sixel_data = renderSixel(resized, palette_size);
    std::cout << "\033[H" << sixel_data;  // Clear and render
    precise_sleep_until(next_frame_time);
}
```

## Performance Optimizations

### Frame Timing Optimizations

- **Adaptive Frame Skipping**: Automatically drops frames when behind schedule
- **High-Resolution Timing**: Nanosecond precision using `std::chrono::high_resolution_clock`
- **Buffered I/O**: Optimized terminal output with buffering control
- **Memory Pre-allocation**: Reserved string buffers to avoid reallocations

### Video Quality Settings

```cpp
// Adaptive scaling based on FPS
double scale_factor = 0.95;               // Default 95% of terminal
if (fps > 30.0) scale_factor = 0.90;      // 90% for high FPS
if (fps > 50.0) scale_factor = 0.85;      // 85% for very high FPS
```

### SIXEL Optimization

- **Palette Size**: Configurable color palette (default: 256 colors)
- **Quality Settings**: Balanced quality/performance ratio
- **Interpolation**: Fast nearest-neighbor resizing for real-time playback

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
    int width = 0;                    // Target width (0 = auto)
    int height = 0;                   // Target height (0 = auto)  
    RenderMode mode = SIXEL;          // Rendering mode
    int paletteSize = 256;            // SIXEL palette size
    bool aspectRatio = true;          // Preserve aspect ratio
    double contrast = 1.0;            // Contrast adjustment
    double brightness = 0.0;          // Brightness adjustment
    CharStyle style = DETAILED;       // ASCII character style
    DitherMode dither = NONE;         // Dithering mode
    double terminalAspectRatio = 0.5; // Terminal character aspect
};
```

### Rendering Modes

```cpp
enum RenderMode {
    SIXEL,        // High-quality SIXEL graphics
    ASCII_COLOR,  // Colored ASCII blocks
    ASCII_GRAY,   // Grayscale ASCII characters
    EXACT         // True-color Unicode blocks
};
```

## Examples

### Programmatic Usage

```cpp
#include "sakura.hpp"

int main() {
    Sakura renderer;
    RenderOptions options;
    
    // Configure for high quality
    options.mode = SIXEL;
    options.paletteSize = 256;
    options.width = 800;
    options.height = 600;
    
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

### Code Standards

- **C++17** standard compliance
- **Google C++ Style Guide** formatting
- **Memory safety** with RAII patterns
- **Performance-first** design principles

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