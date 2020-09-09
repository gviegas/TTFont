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

class Font {
 public:
  explicit Font(const std::string& pathname);
  ~Font();
  Font(const Font&) = delete;
  Font& operator=(const Font&) = delete;
  void printInfo();

 private:
  class Impl;
  std::unique_ptr<Impl> impl;
};

#endif // FONT_FONT_H
