#include "sakura.hpp"
#include <chrono>
#include <cpr/cpr.h>
#include <iostream>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#include <vector>

const std::string Sakura::ASCII_CHARS_SIMPLE = " .:-=+*#%@";
const std::string Sakura::ASCII_CHARS_DETAILED =
    " .'`^\",:;Il!i><~+_-?][}{1)(|\\/"
    "tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
const std::string Sakura::ASCII_CHARS_BLOCKS = " \u2591\u2592\u2593\u2588";

std::pair<int, int> Sakura::getTermSize() {
  struct winsize w;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
    return {w.ws_col, w.ws_row};
  }
  return {80, 24}; // fallback
}

bool Sakura::renderFromUrl(const std::string &url,
                           const RenderOptions &options) {
  auto response = cpr::Get(cpr::Url{url});
  if (response.status_code != 200) {
    std::cerr << "Failed to download image. Status: " << response.status_code
              << std::endl;
    return false;
  }
  std::vector<uchar> imgData(response.text.begin(), response.text.end());
  cv::Mat img = cv::imdecode(imgData, cv::IMREAD_COLOR);
  if (img.empty()) {
    std::cerr << "Failed to decode image" << std::endl;
    return false;
  }
  renderFromMat(img, options);
  return true;
}

bool Sakura::renderFromUrl(const std::string &url) {
  RenderOptions options;
  options.mode = EXACT;
  return renderFromUrl(url, options);
}

void Sakura::renderFromMat(const cv::Mat &img, const RenderOptions &options) {
  cv::Mat adjusted = img.clone();
  if (options.contrast != 1.0 || options.brightness != 0.0) {
    adjusted.convertTo(adjusted, -1, options.contrast * 1.2,
                       options.brightness);
  }

  int target_width = options.width;
  int target_height = options.height;

  if (target_width == 0 || target_height == 0) {
    auto [w, h] = getTerminalSize();
    if (target_width == 0)
      target_width = w;
    if (target_height == 0)
      target_height = h;
  }

  if (options.aspectRatio) {
    double aspectRatio = static_cast<double>(adjusted.cols) / adjusted.rows;
    if (options.mode == EXACT) {
      aspectRatio *= 2.0; // Adjust for 2:1 terminal cell aspect ratio
    }
    if (aspectRatio > static_cast<double>(target_width) / target_height) {
      target_height = static_cast<int>(target_width / aspectRatio);
    } else {
      target_width = static_cast<int>(target_height * aspectRatio);
    }
    target_width = std::max(target_width, 1);
    target_height = std::max(target_height, 1);
  }

  cv::Mat resized;
  if (options.mode == EXACT) {
    cv::resize(adjusted, resized, cv::Size(target_width, target_height * 2), 0,
               0, cv::INTER_AREA);
    renderExact(resized, target_height);
  } else {
    cv::resize(adjusted, resized, cv::Size(target_width, target_height), 0, 0,
               cv::INTER_AREA);
    if (options.mode == ASCII_COLOR) {
      renderAsciiColor(resized);
    } else if (options.mode == ASCII_GRAY) {
      const std::string &charSet = getCharSet(options.style);
      renderAsciiGrayscale(resized, charSet);
    }
  }
}

const std::string &Sakura::getCharSet(CharStyle style) const {
  switch (style) {
  case SIMPLE:
    return ASCII_CHARS_SIMPLE;
  case DETAILED:
    return ASCII_CHARS_DETAILED;
  case BLOCKS:
    return ASCII_CHARS_BLOCKS;
  default:
    return ASCII_CHARS_DETAILED;
  }
}

void Sakura::renderExact(const cv::Mat &resized, int terminal_height) {
  int height = resized.rows / 2;
  int width = resized.cols;
  for (int k = 0; k < std::min(height, terminal_height); k++) {
    for (int j = 0; j < width; j++) {
      cv::Vec3b top_pixel = resized.at<cv::Vec3b>(2 * k, j);
      cv::Vec3b bottom_pixel = (2 * k + 1 < resized.rows)
                                   ? resized.at<cv::Vec3b>(2 * k + 1, j)
                                   : top_pixel;
      int r_top = top_pixel[2];
      int g_top = top_pixel[1];
      int b_top = top_pixel[0];
      int r_bottom = bottom_pixel[2];
      int g_bottom = bottom_pixel[1];
      int b_bottom = bottom_pixel[0];
      printf("\x1b[48;2;%d;%d;%dm\x1b[38;2;%d;%d;%dm▀\x1b[0m", r_bottom,
             g_bottom, b_bottom, r_top, g_top, b_top);
    }
    printf("\n");
  }
  fflush(stdout);
}

void ImageRenderer::renderAsciiColor(const cv::Mat &resized) {
  int height = resized.rows;
  int width = resized.cols;
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      cv::Vec3b pixel = resized.at<cv::Vec3b>(i, j);
      int r = pixel[2];
      int g = pixel[1];
      int b = pixel[0];
      printf("\x1b[38;2;%d;%d;%dm█\x1b[0m", r, g, b);
    }
    printf("\n");
  }
  fflush(stdout);
}

void ImageRenderer::renderAsciiGrayscale(const cv::Mat &resized,
                                         const std::string &charSet) {
  cv::Mat gray;
  cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);
  int height = gray.rows;
  int width = gray.cols;
  int num_chars = charSet.size();
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      int intensity = gray.at<uchar>(i, j);
      int idx = (intensity * (num_chars - 1)) / 255;
      char c = charSet[idx];
      printf("%c", c);
    }
    printf("\n");
  }
  fflush(stdout);
}

bool ImageRenderer::renderGifFromUrl(const std::string &gifUrl,
                                     const RenderOptions &options) {
  cv::VideoCapture cap(gifUrl);
  if (!cap.isOpened()) {
    std::cerr << "Failed to open GIF" << std::endl;
    return false;
  }

  double fps = cap.get(cv::CAP_PROP_FPS);
  int delay = (fps > 0) ? static_cast<int>(1000 / fps) : 100;

  while (true) {
    cv::Mat frame;
    if (!cap.read(frame))
      break;
    std::cout << "\033[2J\033[1;1H"; // Clear terminal
    renderFromMat(frame, options);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
  }

  cap.release();
  return true;
}
