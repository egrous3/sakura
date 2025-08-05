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
    " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
const std::string Sakura::ASCII_CHARS_BLOCKS = " \u2591\u2592\u2593\u2588";

std::pair<int, int> Sakura::getTerminalSize() {
  struct winsize w;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
    return {w.ws_col, w.ws_row};
  }
  return {80, 24};
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
  return renderFromMat(img, options);
}

bool Sakura::renderFromUrl(const std::string &url) {
  RenderOptions options;
  options.mode = EXACT;
  return renderFromUrl(url, options);
}

bool Sakura::renderFromMat(const cv::Mat &img, const RenderOptions &options) {
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
      aspectRatio *= 2.0;
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
    if (resized.empty()) {
      std::cerr << "Resize failed for exact mode" << std::endl;
      return false;
    }
    renderExact(resized, target_height);
  } else {
    cv::resize(adjusted, resized, cv::Size(target_width, target_height), 0, 0,
               cv::INTER_AREA);
    if (resized.empty()) {
      std::cerr << "Resize failed for ASCII mode" << std::endl;
      return false;
    }
    if (options.mode == ASCII_COLOR) {
      renderAsciiColor(resized);
    } else if (options.mode == ASCII_GRAY) {
      const std::string &charSet = getCharSet(options.style);
      renderAsciiGrayscale(resized, charSet, options.dither);
    }
  }
  return true;
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
    std::string line;
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
      char buf[50];
      snprintf(buf, sizeof(buf),
               "\x1b[48;2;%d;%d;%dm\x1b[38;2;%d;%d;%dm▀\x1b[0m", r_bottom,
               g_bottom, b_bottom, r_top, g_top, b_top);
      line += buf;
    }
    printf("%s\n", line.c_str());
  }
  fflush(stdout);
}

void Sakura::renderAsciiColor(const cv::Mat &resized) {
  int height = resized.rows;
  int width = resized.cols;
  for (int i = 0; i < height; i++) {
    std::string line;
    for (int j = 0; j < width; j++) {
      cv::Vec3b pixel = resized.at<cv::Vec3b>(i, j);
      int r = pixel[2];
      int g = pixel[1];
      int b = pixel[0];
      char buf[50];
      snprintf(buf, sizeof(buf), "\x1b[48;2;%d;%d;%dm \x1b[0m", r, g, b);
      line += buf;
    }
    printf("%s\n", line.c_str());
  }
  fflush(stdout);
}

void Sakura::renderAsciiGrayscale(const cv::Mat &resized, const std::string &charSet,
                                  DitherMode dither) {
  cv::Mat gray;
  cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);
  int height = gray.rows;
  int width = gray.cols;
  int num_chars = charSet.size();

  if (dither == FLOYD_STEINBERG) {
    gray.convertTo(gray, CV_32F, 1.0 / 255.0);
    cv::Mat error = cv::Mat::zeros(height, width, CV_32F);
    for (int i = 0; i < height; i++) {
      std::string line;
      for (int j = 0; j < width; j++) {
        float old_value = gray.at<float>(i, j) + error.at<float>(i, j);
        old_value = std::max(0.0f, std::min(1.0f, old_value));
        int level = static_cast<int>(round(old_value * (num_chars - 1)));
        level = std::max(0, std::min(num_chars - 1, level));
        float chosen_value = static_cast<float>(level) / (num_chars - 1);
        float err = old_value - chosen_value;
        if (j + 1 < width) {
          error.at<float>(i, j + 1) += err * 7.0f / 16.0f;
        }
        if (i + 1 < height) {
          if (j - 1 >= 0) {
            error.at<float>(i + 1, j - 1) += err * 3.0f / 16.0f;
          }
          error.at<float>(i + 1, j) += err * 5.0f / 16.0f;
          if (j + 1 < width) {
            error.at<float>(i + 1, j + 1) += err * 1.0f / 16.0f;
          }
        }
        char c = charSet[level];
        line += c;
      }
      printf("%s\n", line.c_str());
    }
  }

  else {
    for (int i = 0; i < height; i++) {
      std::string line;
      for (int j = 0; j < width; j++) {
        int intensity = gray.at<uchar>(i, j);
        int idx = (intensity * (num_chars - 1)) / 255;
        char c = charSet[idx];
        line += c;
      }
      printf("%s\n", line.c_str());
    }
  }
  fflush(stdout);
}

std::vector<std::string>
Sakura::renderImageToLines(const cv::Mat &img, const RenderOptions &options) {
  std::vector<std::string> lines;
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
      aspectRatio *= 2.0;
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
    int height = resized.rows / 2;
    int width = resized.cols;
    for (int k = 0; k < height; k++) {
      std::string line;
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
        char buf[50];
        snprintf(buf, sizeof(buf),
                 "\x1b[48;2;%d;%d;%dm\x1b[38;2;%d;%d;%dm▀\x1b[0m", r_bottom,
                 g_bottom, b_bottom, r_top, g_top, b_top);
        line += buf;
      }
      lines.push_back(line);
    }
  } else if (options.mode == ASCII_COLOR) {
    cv::resize(adjusted, resized, cv::Size(target_width, target_height), 0, 0,
               cv::INTER_AREA);
    int height = resized.rows;
    int width = resized.cols;
    for (int i = 0; i < height; i++) {
      std::string line;
      for (int j = 0; j < width; j++) {
        cv::Vec3b pixel = resized.at<cv::Vec3b>(i, j);
        int r = pixel[2];
        int g = pixel[1];
        int b = pixel[0];
        char buf[50];
        snprintf(buf, sizeof(buf), "\x1b[48;2;%d;%d;%dm \x1b[0m", r, g, b);
        line += buf;
      }
      lines.push_back(line);
    }
  } else if (options.mode == ASCII_GRAY) {
    cv::resize(adjusted, resized, cv::Size(target_width, target_height), 0, 0,
               cv::INTER_AREA);
    const std::string &charSet = getCharSet(options.style);
    int num_chars = charSet.size();
    cv::Mat gray;
    cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);
    if (options.dither == FLOYD_STEINBERG) {
      gray.convertTo(gray, CV_32F, 1.0 / 255.0);
      cv::Mat error = cv::Mat::zeros(gray.rows, gray.cols, CV_32F);
      for (int i = 0; i < gray.rows; i++) {
        std::string line;
        for (int j = 0; j < gray.cols; j++) {
          float old_value = gray.at<float>(i, j) + error.at<float>(i, j);
          old_value = std::max(0.0f, std::min(1.0f, old_value));
          int level = static_cast<int>(round(old_value * (num_chars - 1)));
          level = std::max(0, std::min(num_chars - 1, level));
          float chosen_value = static_cast<float>(level) / (num_chars - 1);
          float err = old_value - chosen_value;
          if (j + 1 < gray.cols) {
            error.at<float>(i, j + 1) += err * 7.0f / 16.0f;
          }
          if (i + 1 < gray.rows) {
            if (j - 1 >= 0) {
              error.at<float>(i + 1, j - 1) += err * 3.0f / 16.0f;
            }
            error.at<float>(i + 1, j) += err * 5.0f / 16.0f;
            if (j + 1 < gray.cols) {
              error.at<float>(i + 1, j + 1) += err * 1.0f / 16.0f;
            }
          }
          line += charSet[level];
        }
        lines.push_back(line);
      }
    } else {
      for (int i = 0; i < gray.rows; i++) {
        std::string line;
        for (int j = 0; j < gray.cols; j++) {
          int intensity = gray.at<uchar>(i, j);
          int idx = (intensity * (num_chars - 1)) / 255;
          line += charSet[idx];
        }
        lines.push_back(line);
      }
    }
  }
  return lines;
}

bool Sakura::renderGridFromUrls(const std::vector<std::string> &urls, int cols,
                                const RenderOptions &options) {
  if (urls.empty() || cols <= 0) {
    std::cerr << "Invalid grid parameters" << std::endl;
    return false;
  }

  int rows = (urls.size() + cols - 1) / cols;
  auto [term_width, term_height] = getTerminalSize();
  int cell_width = term_width / cols;
  int cell_height = term_height / rows;

  std::vector<std::vector<std::string>> all_lines;
  for (const auto &url : urls) {
    auto response = cpr::Get(cpr::Url{url});
    if (response.status_code != 200) {
      std::cerr << "Failed to download image: " << url << std::endl;
      continue;
    }
    std::vector<uchar> imgData(response.text.begin(), response.text.end());
    cv::Mat img = cv::imdecode(imgData, cv::IMREAD_COLOR);
    if (img.empty()) {
      std::cerr << "Failed to decode image: " << url << std::endl;
      continue;
    }
    RenderOptions cell_options = options;
    cell_options.width = cell_width;
    cell_options.height = cell_height;
    all_lines.push_back(renderImageToLines(img, cell_options));
  }

  int max_height = 0;
  for (const auto &lines : all_lines) {
    max_height = std::max(max_height, static_cast<int>(lines.size()));
  }

  for (int i = 0; i < max_height; i++) {
    std::string row_line;
    for (int c = 0; c < cols && c < all_lines.size(); c++) {
      if (i < all_lines[c].size()) {
        row_line += all_lines[c][i];
      } else {
        row_line += std::string(cell_width, ' ');
      }
    }
    printf("%s\n", row_line.c_str());
  }
  fflush(stdout);
  return true;
}

bool Sakura::renderGifFromUrl(const std::string &gifUrl,
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
    std::cout << "\033[2J\033[1;1H";
    renderFromMat(frame, options);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
  }

  cap.release();
  return true;
}
