#ifndef SAKURA_HPP
#define SAKURA_HPP

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

class Sakura {
public:
  enum CharStyle { SIMPLE, DETAILED, BLOCKS };
  enum RenderMode { EXACT, ASCII_COLOR, ASCII_GRAY };
  enum DitherMode { NONE, FLOYD_STEINBERG };

  struct RenderOptions {
    int width = 0;
    int height = 0;
    CharStyle style = SIMPLE;
    RenderMode mode = EXACT;
    DitherMode dither = NONE;
    bool aspectRatio = true;
    double contrast = 1.2;
    double brightness = 0.0;
    double terminalAspectRatio = 2.0;
  };

  bool renderFromUrl(const std::string &url, const RenderOptions &options);
  bool renderFromUrl(const std::string &url);
  bool renderFromMat(const cv::Mat &img, const RenderOptions &options);
  bool renderGridFromUrls(const std::vector<std::string> &urls, int cols,
                          const RenderOptions &options);
  bool renderGifFromUrl(const std::string &gifUrl,
                        const RenderOptions &options);
  std::vector<std::string> renderImageToLines(const cv::Mat &img,
                                              const RenderOptions &options);

private:
  static const std::string ASCII_CHARS_SIMPLE;
  static const std::string ASCII_CHARS_DETAILED;
  static const std::string ASCII_CHARS_BLOCKS;

  const std::string &getCharSet(CharStyle style) const;
  static std::pair<int, int> getTerminalSize();
  std::vector<std::string> renderExact(const cv::Mat &resized,
                                       int terminal_height);
  std::vector<std::string> renderAsciiColor(const cv::Mat &resized);
  std::vector<std::string> renderAsciiGrayscale(const cv::Mat &resized,
                                                const std::string &charSet,
                                                DitherMode dither);
  bool preprocessAndResize(const cv::Mat &img, const RenderOptions &options,
                           cv::Mat &resized, int &target_width,
                           int &target_height);
};

#endif // SAKURA_HPP
