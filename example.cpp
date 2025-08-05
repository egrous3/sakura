#include "sakura.hpp"
#include <iostream>

// example

int main(void) {
  Sakura renderer;
  Sakura::RenderOptions options;
  options.mode = Sakura::ASCII_COLOR;
  options.dither = Sakura::FLOYD_STEINBERG;
  options.width = 100;
  options.height = 50;

  std::string url = "https://cdn.waifu.im/7681.jpg";
  std::string gifUrl = "https://media1.tenor.com/m/lA-2hW5dSpkAAAAd/"
                       "bocchi-the-rock-kita-ikuyo.gif";
  bool stat = renderer.renderGifFromUrl(gifUrl, options);
  if (!stat) {
    std::cerr << "Failed to render image\n";
  }
  return 0;
}
