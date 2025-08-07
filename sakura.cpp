#include "sakura.hpp"
#include <chrono>
#include <cpr/cpr.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <sixel.h>
#include <cstdlib>
#include <future>
#include <atomic>
#include <fstream>
#include <ctime>
#include <iomanip>

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
  if (options.mode == SIXEL) {
    cv::Mat processed;
    
    if (img.cols != options.width || img.rows != options.height) {
      cv::resize(img, processed, cv::Size(options.width, options.height), 0, 0, cv::INTER_NEAREST);
    } else {
      processed = img;
    }
    
    std::string sixelData = renderSixel(processed, options.paletteSize);
    printf("%s", sixelData.c_str());
    fflush(stdout);
    return true;
  }
  
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

  std::string line;
  for (int k = 0; k < std::min(height, terminal_height); k++) {
    line.clear();
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

cv::Mat Sakura::quantizeImage(const cv::Mat &inputImg, int numColors,
                              cv::Mat &palette) {
  cv::Mat sourceImg;
  if (inputImg.channels() == 1) {
    cv::cvtColor(inputImg, sourceImg, cv::COLOR_GRAY2BGR);
  } else {
    sourceImg = inputImg;
  }

  cv::Mat workingImg;
  if (sourceImg.rows * sourceImg.cols > 65536) {
    double scale = sqrt(65536.0 / (sourceImg.rows * sourceImg.cols));
    int newWidth = static_cast<int>(sourceImg.cols * scale);
    int newHeight = static_cast<int>(sourceImg.rows * scale);
    newWidth = std::max(1, newWidth);
    newHeight = std::max(1, newHeight);
    cv::resize(sourceImg, workingImg, cv::Size(newWidth, newHeight), 0, 0,
               cv::INTER_AREA);
  } else {
    workingImg = sourceImg;
  }

  cv::Mat samples = workingImg.reshape(1, workingImg.rows * workingImg.cols);
  samples.convertTo(samples, CV_32F);

  cv::Mat labels;
  cv::Mat centers;
  cv::kmeans(samples, numColors, labels,
             cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT,
                              20, 1.0),
             5, cv::KMEANS_PP_CENTERS, centers);

  centers.convertTo(palette, CV_8UC1);
  palette = palette.reshape(3, numColors);

  cv::Mat quantizedImg(sourceImg.size(), CV_8U);
  for (int y = 0; y < sourceImg.rows; y++) {
    for (int x = 0; x < sourceImg.cols; x++) {
      cv::Vec3b pixel = sourceImg.at<cv::Vec3b>(y, x);

      int bestIdx = 0;
      double minDistSq = std::numeric_limits<double>::max();

      for (int i = 0; i < numColors; i++) {
        cv::Vec3b paletteColor = palette.at<cv::Vec3b>(i);
        double distSq = 0;
        for (int c = 0; c < 3; c++) {
          double diff = pixel[c] - paletteColor[c];
          distSq += diff * diff;
        }
        if (distSq < minDistSq) {
          minDistSq = distSq;
          bestIdx = i;
        }
      }
      quantizedImg.at<uchar>(y, x) = bestIdx;
    }
  }

  return quantizedImg;
}

static int string_writer(char *data, int size, void *priv) {
    auto *sixel_string = static_cast<std::string*>(priv);
    sixel_string->append(data, size);
    return size;
}

std::string Sakura::renderSixel(const cv::Mat &img, int paletteSize) {
    if (img.empty()) {
        return "";
    }

    int optimized_palette = paletteSize;
    
    cv::Mat rgb_img;
    cv::cvtColor(img, rgb_img, cv::COLOR_BGR2RGB);

    sixel_output_t *output = nullptr;
    std::string sixel_output_string;
    if (sixel_output_new(&output, string_writer, &sixel_output_string, nullptr) != SIXEL_OK) {
        return "";
    }

    sixel_dither_t *dither = nullptr;
    if (sixel_dither_new(&dither, optimized_palette, nullptr) != SIXEL_OK || dither == nullptr) {
        sixel_output_unref(output);
        return "";
    }

    if (sixel_dither_initialize(dither, rgb_img.data, rgb_img.cols, rgb_img.rows, SIXEL_PIXELFORMAT_RGB888,
                               SIXEL_LARGE_NORM, SIXEL_REP_CENTER_BOX, SIXEL_QUALITY_HIGH) != SIXEL_OK) {
        sixel_dither_unref(dither);
        sixel_output_unref(output);
        return "";
    }

    if (sixel_encode(rgb_img.data, rgb_img.cols, rgb_img.rows, 3, dither, output) != SIXEL_OK) {
        sixel_dither_unref(dither);
        sixel_output_unref(output);
        return "";
    }

    sixel_dither_unref(dither);
    sixel_output_unref(output);

    return sixel_output_string;
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

  // Get GIF properties
  int gif_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
  int gif_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
  double fps = cap.get(cv::CAP_PROP_FPS);
  if (fps <= 0) fps = 10.0; // Default GIF speed

  std::cout << "GIF: " << gif_width << "x" << gif_height << ", " << fps << " FPS" << std::endl;

  double gifAspect = static_cast<double>(gif_width) / gif_height;
  double termAspect = static_cast<double>(options.width) / options.height;
  
  RenderOptions gifOptions = options;
  
  if (gifAspect > termAspect) {
    gifOptions.width = options.width;
    gifOptions.height = static_cast<int>(options.width / gifAspect);
  } else {
    gifOptions.height = options.height;
    gifOptions.width = static_cast<int>(options.height * gifAspect);
  }
  
  if (fps > 20.0) {
    gifOptions.width = static_cast<int>(gifOptions.width * 0.95);
    gifOptions.height = static_cast<int>(gifOptions.height * 0.95);
  }
  
  std::cout << "Scaled to: " << gifOptions.width << "x" << gifOptions.height << std::endl;

  auto frame_duration_ns = static_cast<long long>(1000000000.0 / fps);
  auto start_time = std::chrono::high_resolution_clock::now();
  
  int frame_number = 0;
  int frames_dropped = 0;
  
  setvbuf(stdout, nullptr, _IONBF, 0);
  std::cout << "\033[2J\033[?25l" << std::flush;

  cv::Mat frame, resized_frame;
  cv::Size target_size(gifOptions.width, gifOptions.height);

  while (true) {
    auto frame_start = std::chrono::high_resolution_clock::now();
    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(frame_start - start_time).count();
    
    long long target_frame = elapsed_ns / frame_duration_ns;
    
    if (frame_number < target_frame) {
      int frames_behind = target_frame - frame_number;  
      if (frames_behind > 2 && frames_dropped < frame_number * 0.3) {
        if (!cap.read(frame)) {
          goto end_gif_playback;
        }
        frame_number++;
        frames_dropped++;
        continue;
      }
    }
    
    if (!cap.read(frame))
      break;
      
    cv::resize(frame, resized_frame, target_size, 0, 0, cv::INTER_NEAREST);
    
    std::string sixel_data = renderSixel(resized_frame, gifOptions.paletteSize);
    std::cout << "\033[H" << sixel_data;
    
    frame_number++;
    
    if (frame_number % 20 == 0) {
      std::cout << "\nGIF: " << frame_number << " D:" << frames_dropped;
    }
    
    auto next_frame_time_ns = start_time + std::chrono::nanoseconds(frame_number * frame_duration_ns);
    auto now = std::chrono::high_resolution_clock::now();
    
    if (next_frame_time_ns > now) {
      auto sleep_duration = next_frame_time_ns - now;
      if (sleep_duration > std::chrono::milliseconds(2)) {
        std::this_thread::sleep_for(sleep_duration - std::chrono::milliseconds(1));
        while (std::chrono::high_resolution_clock::now() < next_frame_time_ns) {
          std::this_thread::yield();
        }
      } else {
        while (std::chrono::high_resolution_clock::now() < next_frame_time_ns) {
          std::this_thread::yield();
        }
      }
    }
  }
  
  end_gif_playback:
  
  std::cout << "\033[?25h" << std::flush;
  setvbuf(stdout, nullptr, _IOLBF, 0);
  
  cap.release();
  
  std::cout << "\nOptimized GIF completed. Frames: " << frame_number 
            << ", Dropped: " << frames_dropped 
            << " (" << std::fixed << std::setprecision(1) 
            << (frames_dropped * 100.0 / frame_number) << "%)" << std::endl;
  return true;
}

bool Sakura::renderVideoFromUrl(const std::string &videoUrl,
                                const RenderOptions &options) {
  auto response = cpr::Get(cpr::Url{videoUrl});
  if (response.status_code != 200) {
    std::cerr << "Failed to download video. Status: " << response.status_code << std::endl;
    return false;
  }
  
  std::string tempFile = "/tmp/sakura_video_" + std::to_string(std::time(nullptr));
  std::ofstream file(tempFile, std::ios::binary);
  file.write(response.text.data(), response.text.size());
  file.close();
  
  bool result = renderVideoFromFile(tempFile, options);
  
  std::remove(tempFile.c_str());
  return result;
}

bool Sakura::renderVideoFromFile(const std::string &videoPath,
                                 const RenderOptions &options) {
  cv::VideoCapture cap(videoPath);
  if (!cap.isOpened()) {
    std::cerr << "Failed to open video: " << videoPath << std::endl;
    return false;
  }

  // Get video properties
  double fps = cap.get(cv::CAP_PROP_FPS);
  int frame_count = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
  int video_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
  int video_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
  double duration = frame_count / fps;
  
  if (fps <= 0) fps = 30.0; // Default fallback
  
  std::cout << "Video: " << video_width << "x" << video_height << ", " 
            << frame_count << " frames, " << fps << " FPS, " 
            << duration << "s duration" << std::endl;

  RenderOptions videoOptions = options;
  
  double scale_factor = 0.95;
  if (fps > 30.0) scale_factor = 0.90;
  if (fps > 50.0) scale_factor = 0.85;
  
  videoOptions.width = static_cast<int>(options.width * scale_factor);
  videoOptions.height = static_cast<int>(options.height * scale_factor);
  
  double videoAspect = static_cast<double>(video_width) / video_height;
  if (videoAspect > 1.0) {
    videoOptions.height = static_cast<int>(videoOptions.width / videoAspect);
  } else {
    videoOptions.width = static_cast<int>(videoOptions.height * videoAspect);
  }
  
  std::cout << "Scaled to: " << videoOptions.width << "x" << videoOptions.height << std::endl;

  cv::Mat frame, resized_frame;
  cv::Size target_size(videoOptions.width, videoOptions.height);
  
  auto frame_duration_ns = static_cast<long long>(1000000000.0 / fps);
  auto start_time = std::chrono::high_resolution_clock::now();
  
  int frame_number = 0;
  int frames_dropped = 0;
  int frames_rendered = 0;
  
  setvbuf(stdout, nullptr, _IONBF, 0);
  std::cout << "\033[2J\033[?25l" << std::flush;
  
  std::atomic<bool> audio_started{false};
  std::future<void> audio_future = std::async(std::launch::async, [&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    audio_started = true;
    std::string audio_cmd = "ffplay -nodisp -autoexit \"" + videoPath + "\" 2>/dev/null";
    std::system(audio_cmd.c_str());
  });
  
  std::string sixel_data;
  sixel_data.reserve(1024 * 1024);
  
  while (true) {
    auto frame_start = std::chrono::high_resolution_clock::now();
    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(frame_start - start_time).count();
    
    long long target_frame_number = elapsed_ns / frame_duration_ns;
    
    if (frame_number < target_frame_number) {
      int frames_behind = target_frame_number - frame_number;
      
      if (frames_behind > 2) {
        while (frame_number < target_frame_number - 1) {
          if (!cap.read(frame)) goto end_playback;
          frame_number++;
          frames_dropped++;
        }
      }
    }
    
    if (!cap.read(frame)) break;
    frame_number++;
    
    cv::resize(frame, resized_frame, target_size, 0, 0, cv::INTER_NEAREST);
    
    sixel_data = renderSixel(resized_frame, videoOptions.paletteSize);
    
    std::cout << "\033[H" << sixel_data;
    frames_rendered++;
    
    if (frame_number % (static_cast<int>(fps) * 3) == 0) {
      std::cout << "\nFrame: " << frame_number << "/" << frame_count 
                << " Dropped: " << frames_dropped << " Audio: " << (audio_started ? "ON" : "OFF");
    }
    
    auto next_frame_time = start_time + std::chrono::nanoseconds(frame_number * frame_duration_ns);
    auto now = std::chrono::high_resolution_clock::now();
    
    if (next_frame_time > now) {
      auto sleep_duration = next_frame_time - now;
      
      if (sleep_duration > std::chrono::milliseconds(1)) {
        std::this_thread::sleep_for(sleep_duration);
      }
    }
  }
  
  end_playback:
  
  std::cout << "\033[?25h" << std::flush;
  setvbuf(stdout, nullptr, _IOLBF, 0);
  cap.release();
  std::system("pkill -f ffplay 2>/dev/null");
  
  double drop_rate = (frames_dropped * 100.0) / frame_number;
  double render_rate = (frames_rendered * 100.0) / frame_number;
  std::cout << "\nPlayback completed. Total: " << frame_number 
            << ", Rendered: " << frames_rendered << " (" << std::fixed << std::setprecision(1) << render_rate << "%)"
            << ", Dropped: " << frames_dropped << " (" << drop_rate << "%)" << std::endl;
  return true;
}
