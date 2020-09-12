//
// Font
// font.h
//
// Copyright (C) 2020 Gustavo C. Viegas.
//

#ifndef FONT_FONT_H
#define FONT_FONT_H

#include <string>
#include <memory>
#include <cstdint>

class Glyph {
 public:
  Glyph();
  virtual ~Glyph();
  Glyph(const Glyph&) = delete;
  Glyph& operator=(const Glyph&) = delete;
  virtual std::pair<uint16_t, uint16_t> extent() const = 0;
  virtual const uint8_t* data() const = 0;
};

class Font {
 public:
  explicit Font(const std::string& pathname);
  ~Font();
  Font(const Font&) = delete;
  Font& operator=(const Font&) = delete;
  std::unique_ptr<Glyph> getGlyph(wchar_t chr, uint16_t pts, uint16_t dpi = 72);

 private:
  class Impl;
  std::unique_ptr<Impl> _impl;
};

#endif // FONT_FONT_H
