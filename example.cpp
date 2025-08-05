#include "sakura.hpp"
#include <iostream>

// example

int main(void) {
  Sakura sakura;
  Sakura::RenderOptions options;
  // options.mode = Sakura::EXACT;
  options.mode = Sakura::ASCII_GRAY;
  options.style = Sakura::DETAILED;
  options.dither = Sakura::FLOYD_STEINBERG;

  std::string url = "https://cdn.waifu.im/7681.jpg";
  std::string gifUrl = "https://media1.tenor.com/m/lA-2hW5dSpkAAAAd/"
                       "bocchi-the-rock-kita-ikuyo.gif";
  // bool stat = sakura.renderGifFromUrl(gifUrl, options);
  // if (!stat) {
  //   std::cerr << "Failed to render image\n";
  // }
  bool stat = sakura.renderFromUrl(url, options);
  if (!stat) {
    std::cerr << "Failed to render image\n";
  }
  return 0;
}
