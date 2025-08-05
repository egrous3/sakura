#include "sakura.hpp"
#include <chrono>
#include <climits>
#include <cpr/cpr.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

const std::string Sakura::ASCII_CHARS_SIMPLE = " .:-=+*#%@";
const std::string Sakura::ASCII_CHARS_DETAILED =
    " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
const std::string Sakura::ASCII_CHARS_BLOCKS = " \u2591\u2592\u2593\u2588";

#ifdef _WIN32
#include <windows.h>
std::pair<int, int> Sakura::getTerminalSize() {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  return {csbi.srWindow.Right - csbi.srWindow.Left + 1,
          csbi.srWindow.Bottom - csbi.srWindow.Top + 1};
}
#else
#include <sys/ioctl.h>
#include <unistd.h>
std::pair<int, int> Sakura::getTerminalSize() {
  struct winsize w;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
    return {w.ws_col, w.ws_row};
  }
  return {80, 24};
}
#endif

bool Sakura::preprocessAndResize(const cv::Mat &img,
                                 const RenderOptions &options, cv::Mat &resized,
                                 int &target_width, int &target_height) {
  cv::Mat adjusted;
  if (options.contrast != 1.0 || options.brightness != 0.0) {
    adjusted = img.clone();
    adjusted.convertTo(adjusted, -1, options.contrast * 1.2,
                       options.brightness);
  } else {
    adjusted = img;
  }

  target_width = options.width;
  target_height = options.height;

  if (target_width == 0 || target_height == 0) {
    auto [w, h] = getTerminalSize();
    if (target_width == 0)
      target_width = w;
    if (target_height == 0)
      target_height = h;
  }

  if (options.aspectRatio) {
    double aspectRatio = static_cast<double>(adjusted.cols) / adjusted.rows;
    if (options.mode == EXACT || options.mode == ASCII_COLOR ||
        options.mode == SIXEL) {
      aspectRatio /= options.terminalAspectRatio;
    }
    if (aspectRatio > static_cast<double>(target_width) / target_height) {
      target_height = static_cast<int>(target_width / aspectRatio);
    } else {
      target_width = static_cast<int>(target_height * aspectRatio);
    }
    target_width = std::max(target_width, 1);
    target_height = std::max(target_height, 1);
  }

  if (options.mode == EXACT) {
    cv::resize(adjusted, resized, cv::Size(target_width, target_height * 2), 0,
               0, cv::INTER_AREA);
  } else {
    cv::resize(adjusted, resized, cv::Size(target_width, target_height), 0, 0,
               cv::INTER_AREA);
  }
  return !resized.empty();
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
  cv::Mat resized;
  int target_width, target_height;
  if (!preprocessAndResize(img, options, resized, target_width,
                           target_height)) {
    std::cerr << "Resize failed" << std::endl;
    return false;
  }

  if (options.mode == EXACT || options.mode == ASCII_COLOR) {
    if (resized.channels() == 1) {
      cv::cvtColor(resized, resized, cv::COLOR_GRAY2BGR);
    }
  }

  if (options.mode == SIXEL) {
    std::string sixelData = renderSixel(resized);
    printf("%s", sixelData.c_str());
    fflush(stdout);
    return true;
  }

  std::vector<std::string> lines;
  if (options.mode == EXACT) {
    lines = renderExact(resized, target_height);
  } else if (options.mode == ASCII_COLOR) {
    lines = renderAsciiColor(resized);
  } else if (options.mode == ASCII_GRAY) {
    const std::string &charSet = getCharSet(options.style);
    lines = renderAsciiGrayscale(resized, charSet, options.dither);
  }

  for (const auto &line : lines) {
    printf("%s\n", line.c_str());
  }
  fflush(stdout);
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

std::vector<std::string> Sakura::renderExact(const cv::Mat &resized,
                                             int terminal_height) const {
  std::vector<std::string> lines;
  int height = resized.rows / 2;
  int width = resized.cols;

  // buffer size: ~45 chars per pixel + some padding
  size_t line_buffer = width * 45;
  std::string line;
  line.reserve(line_buffer);
  for (int k = 0; k < std::min(height, terminal_height); k++) {
    line.clear(); // reusing reserved memory
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
      line.append(buf);
    }
    lines.push_back(line);
  }
  return lines;
}

std::vector<std::string>
Sakura::renderAsciiColor(const cv::Mat &resized) const {
  std::vector<std::string> lines;
  int height = resized.rows;
  int width = resized.cols;
  for (int i = 0; i < height; i++) {
    std::string line;
    line.reserve(width * 20);
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
  return lines;
}

std::vector<std::string>
Sakura::renderAsciiGrayscale(const cv::Mat &resized, const std::string &charSet,
                             DitherMode dither) {
  std::vector<std::string> lines;
  cv::Mat gray;

  if (resized.channels() == 3) {
    cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);
  } else {
    gray = resized;
  }
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
        line += charSet[level];
      }
      lines.push_back(line);
    }
  } else {
    for (int i = 0; i < height; i++) {
      std::string line;
      for (int j = 0; j < width; j++) {
        int intensity = gray.at<uchar>(i, j);
        int idx = (intensity * (num_chars - 1)) / 255;
        line += charSet[idx];
      }
      lines.push_back(line);
    }
  }
  return lines;
}

std::vector<std::string>
Sakura::renderImageToLines(const cv::Mat &img, const RenderOptions &options) {
  cv::Mat resized;
  int target_width, target_height;
  if (!preprocessAndResize(img, options, resized, target_width,
                           target_height)) {
    std::cerr << "Resize failed" << std::endl;
    return {};
  }

  if (options.mode == EXACT || options.mode == ASCII_COLOR) {
    if (resized.channels() == 1) {
      cv::cvtColor(resized, resized, cv::COLOR_GRAY2BGR);
    }
  }

  if (options.mode == EXACT) {
    return renderExact(resized, target_height);
  } else if (options.mode == ASCII_COLOR) {
    return renderAsciiColor(resized);
  } else if (options.mode == ASCII_GRAY) {
    const std::string &charSet = getCharSet(options.style);
    return renderAsciiGrayscale(resized, charSet, options.dither);
  }
  return {};
}

// TODO: i still don't understand completely how this function works, its just a
// textbook implementation. so read about it more

// takes a bgr image, returns a 8-bit single channel img where evey pixel value
// is an index into the output 'palette'
cv::Mat Sakura::quantizeImage(const cv::Mat &inputImg, int numColors,
                              cv::Mat &palette) {
  cv::Mat sourceImg;
  if (inputImg.channels() == 1) {
    cv::cvtColor(inputImg, sourceImg, cv::COLOR_GRAY2BGR);
  } else {
    sourceImg = inputImg;
  }

  cv::Mat smallImg;
  // resizing the img to reduce the number of pixels for k-means
  const int MAX_QUANT_DIM = 256;
  double ratio = static_cast<double>(sourceImg.cols) / sourceImg.rows;
  int w, h;
  if (ratio > 1) {
    w = MAX_QUANT_DIM;
    h = static_cast<int>(w / ratio);
  } else {
    h = MAX_QUANT_DIM;
    w = static_cast<int>(h * ratio);
  }
  w = std::max(1, w);
  h = std::max(1, h);
  cv::resize(sourceImg, smallImg, cv::Size(w, h), 0, 0, cv::INTER_AREA);

  // the image becomes a single column matrix with 3 channels
  cv::Mat samples(smallImg.rows * smallImg.cols, 3, CV_32F);
  for (int y = 0; y < smallImg.rows; y++) {
    for (int x = 0; x < smallImg.cols; x++) {
      samples.at<cv::Vec3f>(y + x * smallImg.rows, 0) =
          smallImg.at<cv::Vec3b>(y, x);
    }
  }

  // k means clustering
  // this function finds 'numColors' clusters centers in the sample data
  // these centers will become our new color palette
  cv::Mat labels;
  cv::kmeans(samples, numColors, labels,
             cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT,
                              10, 1.0),
             3, cv::KMEANS_PP_CENTERS, palette);
  // reshaping the palette and labels
  palette = palette.reshape(3, numColors);
  palette.convertTo(palette, CV_8UC3);

  // map the full-sized original image to new palette
  // labels contains the cluster index for each pixel
  cv::Mat quantizedImg(sourceImg.size(), CV_8U);
  for (int y = 0; y < sourceImg.rows; y++) {
    for (int x = 0; x < sourceImg.cols; x++) {
      cv::Vec3b pixel = sourceImg.at<cv::Vec3b>(y, x);
      int min_dist_sq = INT_MAX;
      int best_idx = 0;
      for (int i = 0; i < palette.rows; i++) {
        cv::Vec3b palette_color = palette.at<cv::Vec3b>(i);
        int dist_sq = pow(pixel[0] - palette_color[0], 2) +
                      pow(pixel[1] - palette_color[1], 2) +
                      pow(pixel[2] - palette_color[2], 2);
        if (dist_sq < min_dist_sq) {
          min_dist_sq = dist_sq;
          best_idx = i;
        }
      }
      quantizedImg.at<uchar>(y, x) = best_idx;
    }
  }
  return quantizedImg;
}

std::string Sakura::renderSixel(const cv::Mat &img, int paletteSize) {
  // quantize the image
  cv::Mat palette;
  cv::Mat indexedImg = quantizeImage(img, paletteSize, palette);

  std::stringstream sixelStream;
  // sixel header
  // dcs (device control string) to start sixel mode
  sixelStream << "\x1BPq";

  for (int i = 0; i < palette.rows; ++i) {
    cv::Vec3b color = palette.at<cv::Vec3b>(i, 0);
    sixelStream << "#" << i << ";2;"
                << static_cast<int>(color[2] * 100.0 / 255.0) << ";" // r
                << static_cast<int>(color[1] * 100.0 / 255.0) << ";" // g
                << static_cast<int>(color[0] * 100.0 / 255.0);       // b
  }

  std::vector<unsigned char> prevSixelColumn(paletteSize, 0);
  int repeatCount = 0; // using RLE: run length encoding for huge speed boost
  // these horizontals bands are 6px high
  for (int y = 0; y < indexedImg.rows; y += 6) {
    repeatCount = 0;
    std::fill(prevSixelColumn.begin(), prevSixelColumn.end(), 0);
    for (int x = 0; x < indexedImg.cols; ++x) {
      std::vector<unsigned char> currentSixelColumn(paletteSize, 0);

      for (int i = 0; i < 6; ++i) {
        if (y + i >= indexedImg.rows)
          continue;
        int paletteIndex = indexedImg.at<uchar>(y + i, x);
        currentSixelColumn[paletteIndex] |= (1 << i);
      }
      if (x > 0 && currentSixelColumn == prevSixelColumn) {
        repeatCount++;
      } else {
        // pattern changed, flush any rle seq
        if (repeatCount > 0) {
          for (int p_idx = 0; p_idx < paletteSize; p_idx++) {
            if (prevSixelColumn[p_idx] != 0) {
              sixelStream << '!' << repeatCount
                          << static_cast<char>(prevSixelColumn[p_idx] + 63);
            }
          }
        }
        repeatCount = 0;

        // emitting the new, different column's data
        for (int p_idx = 0; p_idx < paletteSize; p_idx++) {
          if (currentSixelColumn[p_idx] != 0) {
            sixelStream << '#' << p_idx
                        << static_cast<char>(currentSixelColumn[p_idx] + 63);
          }
        }
        // current column becomes prev column
        prevSixelColumn = currentSixelColumn;
      }
    }
    // after the row is done, flush any pending rle
    if (repeatCount > 0) {
      for (int p_idx = 0; p_idx < paletteSize; p_idx++) {
        if (prevSixelColumn[p_idx] != 0) {
          sixelStream << '!' << repeatCount + 1
                      << static_cast<char>(prevSixelColumn[p_idx] + 63);
        }
      }
    }
    // emit a 'line feed' character "-";
    sixelStream << "-";
  }
  // string terminator
  sixelStream << "\x1B\\";
  return sixelStream.str();
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
