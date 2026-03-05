#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace browser::render {

struct Color {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
};

class Canvas {
 public:
  static constexpr std::size_t kMaxPixelCount = 64U * 1024U * 1024U * 3U;

  Canvas() = default;

  Canvas(int width, int height) {
    resize(width, height);
  }

  void resize(int width, int height) {
    width_ = std::max(width, 0);
    height_ = std::max(height, 0);

    const std::size_t width_size = static_cast<std::size_t>(width_);
    const std::size_t height_size = static_cast<std::size_t>(height_);
    constexpr std::size_t kChannels = 3U;

    if (width_size == 0U || height_size == 0U) {
      pixels_.clear();
      return;
    }

    if (width_size > (std::numeric_limits<std::size_t>::max() / kChannels) / height_size) {
      width_ = 0;
      height_ = 0;
      pixels_.clear();
      return;
    }

    const std::size_t pixel_count = width_size * height_size * kChannels;
    if (pixel_count > kMaxPixelCount) {
      width_ = 0;
      height_ = 0;
      pixels_.clear();
      return;
    }
    pixels_.assign(pixel_count, 0);
  }

  [[nodiscard]] int width() const {
    return width_;
  }

  [[nodiscard]] int height() const {
    return height_;
  }

  [[nodiscard]] bool empty() const {
    return width_ <= 0 || height_ <= 0 || pixels_.empty();
  }

  [[nodiscard]] const std::vector<std::uint8_t>& pixels() const {
    return pixels_;
  }

  [[nodiscard]] std::vector<std::uint8_t>& pixels() {
    return pixels_;
  }

  void set_pixel(int x, int y, Color color) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
      return;
    }

    const std::size_t pixel_index =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)) * 3U;
    pixels_[pixel_index + 0] = color.r;
    pixels_[pixel_index + 1] = color.g;
    pixels_[pixel_index + 2] = color.b;
  }

  void clear(Color color) {
    fill_rect(0, 0, width_, height_, color);
  }

  void fill_rect(int x, int y, int width, int height, Color color) {
    if (width_ <= 0 || height_ <= 0 || width <= 0 || height <= 0) {
      return;
    }

    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(width_, x + width);
    const int y1 = std::min(height_, y + height);

    if (x0 >= x1 || y0 >= y1) {
      return;
    }

    for (int py = y0; py < y1; ++py) {
      for (int px = x0; px < x1; ++px) {
        set_pixel(px, py, color);
      }
    }
  }

 private:
  int width_ = 0;
  int height_ = 0;
  std::vector<std::uint8_t> pixels_;
};

}  // namespace browser::render
