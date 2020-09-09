//
// Font
// test.cc
//
// Copyright (C) 2020 Gustavo C. Viegas.
//

#include <iostream>

#include "font.h"

int main(int argc, char* argv[]) {
  std::wcout << "[Font] test\n\n";
  for (int i = 0; i < argc; ++i)
    std::wcout << argv[i] << " ";
  std::wcout << "\n\n";

  try {
    Font font{std::getenv("FONT")};
    font.printInfo();
  } catch (...) {
    std::wcerr << "ERR: 'FONT' env. not defined\n";
    return -1;
  }

  std::wcout << "\n-- end of test --" << std::endl;
  return 0;
}
