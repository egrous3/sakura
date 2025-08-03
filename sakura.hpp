#ifndef SAKURA_HPP
#define SAKURA_HPP

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

class Sakura {
public:
  enum CharStyle { SIMPLE, DETAILED, BLOCKS };
  enum RenderMode { EXACT, ASCII_COLOR, ASCII_GRAY };

  struct RenderOptions {
    int width = 0; // auto detect :)
    int height = 0;
    CharStyle style = DETAILED;
    RenderMode mode = EXACT;
    bool aspectRatio = true;
    double contrast = 1.2;
    double brightness = 0.0;
  };

  bool renderFromUrl(const std::string &url, const RenderOptions &options);
  bool renderFromUrl(const std::string &url);
  bool renderFromMat(const cv::Mat &img, const RenderOptions &options);
  bool renderGifFromUrl(const std::string &gifUrl,
                        const RenderOptions &options);

private:
  static const std::string ASCII_CHARS_SIMPLE;
  static const std::string ASCII_CHARS_DETAILED;
  static const std::string ASCII_CHARS_BLOCKS;

  const std::string &getCharSet(CharStyle style) const;
  static std::pair<int, int> getTermSize();
  void renderExact(const cv::Mat &resized, int terminal_height);
  void renderAsciiColor(const cv::Mat &resized);
  void renderAsciiGrayScale(const cv::Mat &resized, const std::string &charSet);
};

#endif // SAKURA_HPP
