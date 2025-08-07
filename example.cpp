#include <sys/ioctl.h>
#include <unistd.h>
#include <utility>
#include <cpr/cpr.h>
#include <opencv2/opencv.hpp>
#include "sakura.hpp"
#include <iostream>

// Helper to get terminal pixel size (returns {width, height} in pixels)
std::pair<int, int> getTerminalPixelSize() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_xpixel > 0 && w.ws_ypixel > 0) {
        return {w.ws_xpixel, w.ws_ypixel};
    }
    // Fallback: return a reasonable default if pixel size is not available
    return {1920, 1080};
}

// Helper to calculate best-fit dimensions for content
std::pair<int, int> calculateBestFitSize(int contentWidth, int contentHeight, int termWidth, int termHeight) {
    double contentAspect = static_cast<double>(contentWidth) / contentHeight;
    double termAspect = static_cast<double>(termWidth) / termHeight;
    
    int outw, outh;
    if (contentAspect > termAspect) {
        // Content is wider - fit to terminal width
        outw = termWidth;
        outh = static_cast<int>(termWidth / contentAspect);
    } else {
        // Content is taller - fit to terminal height
        outh = termHeight;
        outw = static_cast<int>(termHeight * contentAspect);
    }
    return {outw, outh};
}

int main(void) {
  Sakura sakura;
  
  std::string url, gifUrl, videoUrl, localVideoPath;

  // Get terminal pixel size once
  auto [termPixW, termPixH] = getTerminalPixelSize();

  std::cout << "Sakura Video Player with SIXEL\n";
  std::cout << "1. Image\n2. GIF\n3. Video (URL)\n4. Video (File)\n";
  std::cout << "Choose option (1-4): ";
  
  int choice;
  std::cin >> choice;
  
  bool stat = false;
  switch(choice) {
    case 1: {
      std::cout << "Enter image URL: ";
      std::cin >> url;
      std::cout << "Rendering image...\n";
      // Download image and calculate its dimensions
      auto response = cpr::Get(cpr::Url{url});
      if (response.status_code != 200) {
        std::cerr << "Failed to download image. Status: " << response.status_code << std::endl;
        return 1;
      }
      std::vector<uchar> imgData(response.text.begin(), response.text.end());
      cv::Mat img = cv::imdecode(imgData, cv::IMREAD_COLOR);
      if (img.empty()) {
        std::cerr << "Failed to decode image" << std::endl;
        return 1;
      }
      
      // Calculate proper dimensions for this image
      auto [outw, outh] = calculateBestFitSize(img.cols, img.rows, termPixW, termPixH);
      
      Sakura::RenderOptions options;
      options.mode = Sakura::SIXEL;
      options.dither = Sakura::FLOYD_STEINBERG;
      options.terminalAspectRatio = 1.0;
      options.width = outw;
      options.height = outh;
      
      stat = sakura.renderFromMat(img, options);
      break;
    }
    case 2: {
      std::cout << "Enter GIF URL: ";
      std::cin >> gifUrl;
      std::cout << "Rendering GIF...\n";
      // For GIF, we'll let the sakura library handle sizing internally
      // but still set reasonable defaults
      Sakura::RenderOptions options;
      options.mode = Sakura::SIXEL;
      options.dither = Sakura::FLOYD_STEINBERG;
      options.terminalAspectRatio = 1.0;
      options.width = termPixW;
      options.height = termPixH;
      
      stat = sakura.renderGifFromUrl(gifUrl, options);
      break;
    }
    case 3: {
      std::cout << "Enter video URL: ";
      std::cin >> videoUrl;
      std::cout << "Rendering video from URL (with audio)...\n";
      Sakura::RenderOptions options;
      options.mode = Sakura::SIXEL;
      options.dither = Sakura::FLOYD_STEINBERG;
      options.terminalAspectRatio = 1.0;
      options.width = termPixW;
      options.height = termPixH;
      
      stat = sakura.renderVideoFromUrl(videoUrl, options);
      break;
    }
    case 4: {
      std::cout << "Enter video file path: ";
      std::cin >> localVideoPath;
      std::cout << "Rendering video from file (with audio)...\n";
      
      Sakura::RenderOptions options;
      options.mode = Sakura::SIXEL;
      options.dither = Sakura::FLOYD_STEINBERG;
      options.terminalAspectRatio = 1.0;
      options.width = termPixW;
      options.height = termPixH;
      
      stat = sakura.renderVideoFromFile(localVideoPath, options);
      break;
    }
    default: {
      std::cout << "Invalid choice. Rendering image by default.\n";
      std::cout << "Enter image URL: ";
      std::cin >> url;
      std::cout << "Rendering image...\n";
      // Download image and calculate its dimensions
      auto response = cpr::Get(cpr::Url{url});
      if (response.status_code != 200) {
        std::cerr << "Failed to download image. Status: " << response.status_code << std::endl;
        return 1;
      }
      std::vector<uchar> imgData(response.text.begin(), response.text.end());
      cv::Mat img = cv::imdecode(imgData, cv::IMREAD_COLOR);
      if (img.empty()) {
        std::cerr << "Failed to decode image" << std::endl;
        return 1;
      }
      
      auto [outw, outh] = calculateBestFitSize(img.cols, img.rows, termPixW, termPixH);
      
      Sakura::RenderOptions options;
      options.mode = Sakura::SIXEL;
      options.dither = Sakura::FLOYD_STEINBERG;
      options.terminalAspectRatio = 1.0;
      options.width = outw;
      options.height = outh;
      
      stat = sakura.renderFromMat(img, options);
      break;
    }
  }
  
  if (!stat) {
    std::cerr << "Failed to render content\n";
  }
  return 0;
}
