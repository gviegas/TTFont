//
// Font
// font.cc
//
// Copyright (C) 2020 Gustavo C. Viegas.
//

#include <cstdint>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include "font.h"

#ifdef FONT_DEVEL
# include <iostream>
#endif

#ifdef _DEFAULT_SOURCE
# include <endian.h>
inline int16_t htobe(int16_t v) { return htobe16(v); }
inline uint16_t htobe(uint16_t v) { return htobe16(v); }
inline int32_t htobe(int32_t v) { return htobe32(v); }
inline uint32_t htobe(uint32_t v) { return htobe32(v); }
inline int16_t betoh(int16_t v) { return be16toh(v); }
inline uint16_t betoh(uint16_t v) { return be16toh(v); }
inline int32_t betoh(int32_t v) { return be32toh(v); }
inline uint32_t betoh(uint32_t v) { return be32toh(v); }
#else
// TODO
# error "Invalid platform"
#endif

namespace {

/// Produces tags that identifies 'sfnt' tables (a FourCC).
///
constexpr uint32_t makeTag(char c1, char c2, char c3, char c4) {
  return c4 | (c3 << 8) | (c2 << 16) | (c1 << 24);
}

/// Font manager for 'sfnt' font files (TrueType outline).
///
class SFNT {
 public:
  SFNT(std::ifstream& ifs) {
    if (!verify(ifs))
      // TODO
      std::abort();
    if (!load(ifs))
      // TODO
      std::abort();
  }

  // TODO
  void scan(wchar_t glyph) {
    std::vector<Component> comps;
    fetch(glyph, comps);
  }

 private:
  /// Font directory.
  ///
  struct DirSub {
    uint32_t scaler;
    uint16_t tabN;
    uint16_t srchRng;
    uint16_t entSel;
    uint16_t rngShft;
  };
  struct DirEntry {
    uint32_t tag;
    uint32_t csum;
    uint32_t off;
    uint32_t len;
  };
  static constexpr uint32_t DirSubLen = 12;
  static constexpr uint32_t DirEntryLen = 16;
  static_assert(sizeof(DirSub) == DirSubLen, "!sizeof");
  static_assert(sizeof(DirEntry) == DirEntryLen, "!sizeof");

  /// Font header table.
  ///
  struct Head {
    int32_t version;
    int32_t revision;
    uint32_t csumAdj;
    uint32_t magic;
    uint16_t flags;
    uint16_t upem;
    uint32_t creDate[2];
    uint32_t modDate[2];
    int16_t xMin;
    int16_t yMin;
    int16_t xMax;
    int16_t yMax;
    uint16_t style;
    uint16_t lowestPpem;
    int16_t dirHint;
    int16_t locaFmt;
    int16_t glyfFmt;
  };
  static constexpr uint32_t HeadLen = 54;
  static_assert(offsetof(Head, glyfFmt) == HeadLen-2, "!offsetof");

  /// Maximum profile table.
  ///
  struct Maxp {
    int32_t version;
    uint16_t glyphN;
    uint16_t maxPts;
    uint16_t maxCntrs;
    uint16_t maxCompPts;
    uint16_t maxCompCntrs;
    uint16_t maxZones;
    uint16_t maxZ0Pts;
    uint16_t maxStorages;
    uint16_t maxFDefs;
    uint16_t maxIDefs;
    uint16_t maxStackElems;
    uint16_t maxInstrSz;
    uint16_t maxCompElems;
    uint16_t maxCompDepth;
  };
  static constexpr uint32_t MaxpLen = 32;
  static_assert(offsetof(Maxp, maxCompDepth) == MaxpLen-2, "!offsetof");

  /// Character to glyph mapping table.
  ///
  struct CmapIndex {
    uint16_t version;
    uint16_t subN;
  };
  struct CmapEncoding {
    uint16_t platfID;
    uint16_t specID;
    uint32_t off;
  };
  struct Cmap4 {
    uint16_t fmt;
    uint16_t len;
    uint16_t lang;
    uint16_t segCount2x;
    uint16_t srchRng;
    uint16_t entSel;
    uint16_t rngShft;
    // XXX: Variable data follows.
  };
  struct Cmap6 {
    uint16_t fmt;
    uint16_t len;
    uint16_t lang;
    uint16_t firstCode;
    uint16_t entN;
    // XXX: u16[entN] follows.
  };
  static constexpr uint32_t CmapIndexLen = 4;
  static constexpr uint32_t CmapEncodingLen = 8;
  static constexpr uint32_t Cmap4Len = 14;
  static constexpr uint32_t Cmap6Len = 10;
  static_assert(sizeof(CmapIndex) == CmapIndexLen, "!sizeof");
  static_assert(sizeof(CmapEncoding) == CmapEncodingLen, "!sizeof");
  static_assert(sizeof(Cmap4) == Cmap4Len, "!sizeof");
  static_assert(sizeof(Cmap6) == Cmap6Len, "!sizeof");

  /// Glyph data table.
  ///
  struct Glyf {
    int16_t cntrN;
    int16_t xMin;
    int16_t yMin;
    int16_t xMax;
    int16_t yMax;
    // XXX: Either simple or compound glyph description follows.
  };
  static constexpr uint32_t GlyfLen = 10;
  static_assert(sizeof(Glyf) == GlyfLen, "!sizeof");
  static_assert(alignof(Glyf) == alignof(int16_t), "!alignof");

  /// Table tags.
  ///
  enum Tag : uint32_t {
    CmapTag = ::makeTag('c', 'm', 'a', 'p'),
    GlyfTag = ::makeTag('g', 'l', 'y', 'f'),
    HeadTag = ::makeTag('h', 'e', 'a', 'd'),
    HheaTag = ::makeTag('h', 'h', 'e', 'a'), // TODO
    HmtxTag = ::makeTag('h', 'm', 't', 'x'), // TODO
    LocaTag = ::makeTag('l', 'o', 'c', 'a'),
    MaxpTag = ::makeTag('m', 'a', 'x', 'p'),
    NameTag = ::makeTag('n', 'a', 'm', 'e'), // TODO
    PostTag = ::makeTag('p', 'o', 's', 't')  // TODO
  };

  /// Verifies file.
  ///
  bool verify(std::ifstream& ifs) {
    auto calcCsum = [](const uint32_t* tab, uint32_t len) -> uint32_t {
      uint32_t sum = 0;
      uint32_t dwordN = (len + 3) / 4;
      while (dwordN-- > 0)
        sum += betoh(*tab++);
      return sum;
    };

    ifs.seekg(0);
    DirSub sub;
    ifs.read(reinterpret_cast<char*>(&sub), DirSubLen);

    const uint16_t tabN = betoh(sub.tabN);
    std::vector<DirEntry> ents;
    ents.resize(tabN);
    ifs.read(reinterpret_cast<char*>(ents.data()), tabN*DirEntryLen);

    for (const auto& e : ents) {
      uint32_t off = betoh(e.off);
      uint32_t len = betoh(e.len);
      ifs.seekg(off);
      auto buf = std::make_unique<uint32_t[]>((len+3)/4);
      ifs.read(reinterpret_cast<char*>(buf.get()), ((len+3)/4)*4);
      uint32_t csum = calcCsum(buf.get(), len);
      if (csum != betoh(e.csum) && betoh(e.tag) != HeadTag)
        return false;
    }

    return true;
  }

  /// Loads font data.
  ///
  bool load(std::ifstream& ifs) {
    ifs.seekg(0);
    DirSub sub;
    ifs.read(reinterpret_cast<char*>(&sub), DirSubLen);

    const uint16_t tabN = betoh(sub.tabN);
    std::vector<DirEntry> ents;
    ents.resize(tabN);
    ifs.read(reinterpret_cast<char*>(ents.data()), tabN*DirEntryLen);

    int16_t cmapIdx, glyfIdx, headIdx, locaIdx, maxpIdx;
    cmapIdx = glyfIdx = headIdx = locaIdx = maxpIdx = -1;

    for (uint16_t i = 0; i < ents.size(); ++i) {
      switch (betoh(ents[i].tag)) {
        case CmapTag: cmapIdx = i; break;
        case GlyfTag: glyfIdx = i; break;
        case HeadTag: headIdx = i; break;
        case LocaTag: locaIdx = i; break;
        case MaxpTag: maxpIdx = i; break;
      }
    }

    // the bare minimum for a TrueType
    if (cmapIdx < 0 || glyfIdx < 0 || headIdx < 0 || locaIdx < 0 || maxpIdx < 0)
      return false;

    Head head;
    ifs.seekg(betoh(ents[headIdx].off));
    ifs.read(reinterpret_cast<char*>(&head), betoh(ents[headIdx].len));
    this->upem = betoh(head.upem);
    this->xMin = betoh(head.xMin);
    this->yMin = betoh(head.yMin);
    this->xMax = betoh(head.xMax);
    this->yMax = betoh(head.yMax);

    Maxp maxp;
    ifs.seekg(betoh(ents[maxpIdx].off));
    ifs.read(reinterpret_cast<char*>(&maxp), betoh(ents[maxpIdx].len));
    this->glyphN = betoh(maxp.glyphN);
    this->maxPts = betoh(maxp.maxPts);
    this->maxCntrs = betoh(maxp.maxCntrs);
    this->maxCompPts = betoh(maxp.maxCompPts);
    this->maxCompCntrs = betoh(maxp.maxCompCntrs);

    CmapIndex cmi;
    ifs.seekg(betoh(ents[cmapIdx].off));
    ifs.read(reinterpret_cast<char*>(&cmi), CmapIndexLen);

    const uint16_t cmeN = betoh(cmi.subN);
    std::vector<CmapEncoding> cmes;
    cmes.resize(cmeN);
    ifs.read(reinterpret_cast<char*>(cmes.data()), cmeN*CmapEncodingLen);

    // encodings: Unicode (sparse), Macintosh (roman, trimmed), Windows (sparse)
    const struct {
      uint16_t platfID, specID, fmt, lang;
    } encods[] = {{0, 3, 4, 0}, {1, 0, 6, 0}, {3, 1, 4, 0}};

    // set the cmap member
    auto setMapping = [&](const CmapEncoding& cme, uint16_t fmt) {
      switch (fmt) {
        // sparse format
        case 4: {
          Cmap4 cm4;
          ifs.seekg(betoh(ents[cmapIdx].off) + betoh(cme.off));
          ifs.read(reinterpret_cast<char*>(&cm4), Cmap4Len);
          const uint16_t segCount = betoh(cm4.segCount2x) / 2;
          const uint16_t len = betoh(cm4.len);
          const uint16_t subLen = len > Cmap4Len ? len-Cmap4Len : Cmap4Len-len;
          auto var = std::make_unique<uint16_t[]>(subLen/2);
          ifs.read(reinterpret_cast<char*>(var.get()), subLen);
          uint16_t endCode, startCode, code, delta, rngOff, idx;
          for (uint16_t i = 0; var[i] != 0xFFFF; ++i) {
            endCode = betoh(var[i]);
            startCode = code = betoh(var[segCount+i+1]);
            delta = betoh(var[2*segCount+i+1]);
            rngOff = betoh(var[3*segCount+i+1]);
            if (rngOff != 0) {
              do {
                idx = var[3*segCount+i+1 + rngOff/2 + (code-startCode)];
                idx = betoh(idx);
                this->cmap.emplace(code, idx);
              } while (code++ < endCode);
            } else {
              do {
                idx = delta + code;
                this->cmap.emplace(code, idx);
              } while (code++ < endCode);
            }
          }
        } break;
        // trimmed format
        case 6: {
          Cmap6 cm6;
          ifs.seekg(betoh(ents[cmapIdx].off) + betoh(cme.off));
          ifs.read(reinterpret_cast<char*>(&cm6), Cmap6Len);
          const uint16_t firstCode = betoh(cm6.firstCode);
          const uint16_t entN = betoh(cm6.entN);
          uint16_t idx;
          for (uint16_t c = firstCode; c < entN; ++c) {
            ifs.read(reinterpret_cast<char*>(&idx), sizeof idx);
            this->cmap.emplace(c, betoh(idx));
          }
        } break;
      }
    };

    // find a suitable encoding
    for (const auto& enc : encods) {
      for (const auto& cme : cmes) {
        if (betoh(cme.platfID) != enc.platfID ||
          betoh(cme.specID) != enc.specID)
        { continue; }
        uint16_t v;
        ifs.seekg(betoh(ents[cmapIdx].off) + betoh(cme.off));
        ifs.read(reinterpret_cast<char*>(&v), sizeof v);
        if (betoh(v) != enc.fmt)
          continue;
        ifs.seekg(2, std::ios_base::cur);
        ifs.read(reinterpret_cast<char*>(&v), sizeof v);
        if (betoh(v) != enc.lang)
          continue;
        setMapping(cme, enc.fmt);
        break;
      }
      if (!this->cmap.empty())
        break;
    }

    // glyph offsets stored pre-multiplied/byte-swapped
    this->loca = std::make_unique<uint32_t[]>(this->glyphN+1);
    ifs.seekg(betoh(ents[locaIdx].off));
    if (head.locaFmt == 0) {
      uint16_t v;
      for (uint16_t i = 0; i < this->glyphN+1; ++i) {
        ifs.read(reinterpret_cast<char*>(&v), 2);
        this->loca[i] = 2 * betoh(v);
      }
    } else {
      for (uint16_t i = 0; i < this->glyphN+1; ++i) {
        ifs.read(reinterpret_cast<char*>(&this->loca[i]), 4);
        this->loca[i] = betoh(this->loca[i]);
      }
    }

    // glyph descriptions stored as raw data
    const uint32_t glyfLen = (betoh(ents[glyfIdx].len) + 1) & ~1;
    this->glyf = std::make_unique<uint8_t[]>(glyfLen);
    ifs.seekg(betoh(ents[glyfIdx].off));
    ifs.read(reinterpret_cast<char*>(this->glyf.get()), glyfLen);

    return true;
  }

  /// Component of a glyph (simple glyph).
  ///
  struct Component {
    std::vector<uint16_t> cntrEnd; // last point indices, one per contour
    std::vector<std::tuple<bool, int16_t, int16_t>> pts; // <on curve, x, y>
  };

  /// Checks whether a glyph is made of parts (compound/composite).
  ///
  bool isCompound(uint16_t glyphIdx) {
    auto glyf = reinterpret_cast<Glyf*>(&this->glyf[this->loca[glyphIdx]]);
    return betoh(glyf->cntrN) < 0;
  }

  /// Fetches glyph data.
  ///
  void fetch(wchar_t glyph, std::vector<Component>& comps) {
    const auto it = this->cmap.find(glyph);
    if (it == this->cmap.end()) return;
    const uint16_t idx = it->second;

#ifdef FONT_DEVEL
    std::wcout << "glyph is '" << glyph << "'" << std::endl;
    std::wcout << "idx is " << idx << std::endl;
    const uint32_t off = this->loca[idx];
    std::wcout << "off is " << off << std::endl;
    const auto glyf = reinterpret_cast<Glyf*>(&this->glyf[off]);
    std::wcout << "cntrN is " << betoh(glyf->cntrN) << std::endl;
    std::wcout << "xMin is " << betoh(glyf->xMin) << std::endl;
    std::wcout << "yMin is " << betoh(glyf->yMin) << std::endl;
    std::wcout << "xMax is " << betoh(glyf->xMax) << std::endl;
    std::wcout << "yMax is " << betoh(glyf->yMax) << std::endl;
#endif

    if (isCompound(idx)) {
      fetchCompound(idx, comps);
    } else {
      comps.push_back(Component{});
      fetchSimple(idx, comps.back());
    }

#ifdef FONT_DEVEL
    std::wcout << "\n~~ Components ~~\n" << comps.size() << std::endl;
    std::for_each(comps.begin(), comps.end(),
      [](auto& comp) {
        std::wcout << "\ncntrEnd:\n";
        std::for_each(comp.cntrEnd.begin(), comp.cntrEnd.end(),
          [](const auto& ce) {
            std::wcout << ce << std::endl;
          });
        std::wcout << "\npts:\n";
        std::for_each(comp.pts.begin(), comp.pts.end(),
          [] (const auto& pt) {
            std::wcout <<
              (std::get<0>(pt) ? "on " : "off ") <<
              std::get<1>(pt) << " " <<
              std::get<2>(pt) << "\n";
          });
      });
    std::wcout << "\n~~~~\n";
#endif
  }

  /// Fetches a compound glyph.
  ///
  void fetchCompound(uint16_t glyphIdx, std::vector<Component>& comps) {
    uint16_t itOff = comps.size();
    uint32_t curOff = this->loca[glyphIdx] + sizeof(Glyf);

    auto getWord = [&] {
      int16_t wd = this->glyf[curOff++] << 8;
      wd |= this->glyf[curOff++];
      return wd;
    };

    uint16_t flags;
    uint16_t idx;
    int16_t arg1, arg2;
    int16_t a, b, c, d;

    // iterate over each component
    do {
      flags = getWord();
      idx = getWord();

      if (isCompound(idx)) {
        fetchCompound(idx, comps);
      } else {
        comps.push_back(Component{});
        fetchSimple(idx, comps.back());
      }

      if (flags & 1) {
        // arg1 & arg2 are 2-bytes long
        arg1 = getWord();
        arg2 = getWord();
      } else {
        // arg1 & arg2 are 1-byte long
        arg1 = this->glyf[curOff++];
        arg2 = this->glyf[curOff++];
      }

      if (flags & 2) {
        // args are xy values
        std::for_each(comps.begin()+itOff, comps.end(), [&](auto& comp) {
          std::for_each(comp.pts.begin(), comp.pts.end(), [&](auto& pt) {
            std::get<1>(pt) += arg1;
            std::get<2>(pt) += arg2;
          });
        });
      } else {
        // args are points
        // TODO
        std::abort();
      }

      // TODO: Apply transform.
      if (flags & 8) {
        // simple scale
        a = d = getWord();
        b = c = 0;
      } else if (flags & 64) {
        // different scales
        a = getWord();
        d = getWord();
        b = c = 0;
      } else if (flags & 128) {
        // 2x2 transform
        a = getWord();
        b = getWord();
        c = getWord();
        d = getWord();
      } else {
        // as is
        a = d = 1;
        b = c = 0;
      }

      ++itOff;
    } while (flags & 32);
  }

  /// Fetches a simple glyph.
  ///
  void fetchSimple(uint16_t glyphIdx, Component& comp) {
    uint32_t curOff = this->loca[glyphIdx];
    const Glyf* glyf = reinterpret_cast<Glyf*>(&this->glyf[curOff]);
    curOff += sizeof(Glyf);
    const int16_t cntrN = betoh(glyf->cntrN); // assuming >= 0

    uint16_t* endPts = reinterpret_cast<uint16_t*>(&this->glyf[curOff]);
    curOff += cntrN * sizeof(uint16_t);
    for (uint16_t i = 0; i < cntrN; ++i)
      comp.cntrEnd.push_back(betoh(endPts[i]));
    const uint16_t lastPt = comp.cntrEnd.back();

    uint16_t instrLen = *reinterpret_cast<uint16_t*>(&this->glyf[curOff]);
    curOff += sizeof(uint16_t) + betoh(instrLen);

    // XXX: Cannot assume 2-byte alignment after this point.

    const uint32_t flagStart = curOff;
    int32_t flagN = lastPt;
    uint8_t flags;
    uint8_t repeatN;
    uint32_t yOff = 0;

    // iterate over flags array to compute offsets
    do {
      flags = this->glyf[curOff++];
      repeatN = (flags & 8) ? this->glyf[curOff++] : 0;
      flagN -= repeatN+1;
      if (flags & 2)
        // x is byte
        yOff += repeatN+1;
      else if (!(flags & 16))
        // x is word
        yOff += (repeatN+1)*2;
      // x is same otherwise
    } while (flagN >= 0);

    uint32_t xOff = curOff;
    yOff += curOff;
    curOff = flagStart;
    flagN = lastPt;
    bool onCurve;
    int16_t x = 0;
    int16_t y = 0;
    int16_t dt;

    // iterate again to fetch point data
    do {
      flags = this->glyf[curOff++];
      repeatN = (flags & 8) ? this->glyf[curOff++] : 0;
      flagN -= repeatN+1;

      do {
        onCurve = flags & 1;

        if (flags & 2) {
          if (flags & 16)
            // x is byte, positive sign
            x += this->glyf[xOff++];
          else
            // x is byte, negative sign
            x += -(this->glyf[xOff++]);
        } else if (!(flags & 16)) {
          // x is signed word
          dt = this->glyf[xOff+1];
          dt = (dt << 8) | this->glyf[xOff];
          x += betoh(dt);
          xOff += 2;
        }

        if (flags & 4) {
          if (flags & 32)
            // y is byte, positive sign
            y += this->glyf[yOff++];
          else
            // y is byte, negative sign
            y += -(this->glyf[yOff++]);
        } else if (!(flags & 32)) {
          // y is signed word
          dt = this->glyf[yOff+1];
          dt = (dt << 8) | this->glyf[yOff];
          y += betoh(dt);
          yOff += 2;
        }

        comp.pts.push_back({onCurve, x, y});
      } while (repeatN-- > 0);

    } while (flagN >= 0);
  }

  /// Units per em.
  ///
  uint16_t upem;

  /// Glyph boundaries.
  ///
  int16_t xMin, yMin, xMax, yMax;

  /// Number of glyphs in the font.
  ///
  uint16_t glyphN;

  /// Limits for simple and composite glyphs.
  ///
  uint16_t maxPts, maxCntrs, maxCompPts, maxCompCntrs;

  /// Character code to glyph index mapping.
  ///
  std::unordered_map<uint16_t, uint16_t> cmap;

  /// Location of each glyph in the 'glyf' table, sorted by glyph index.
  ///
  std::unique_ptr<uint32_t[]> loca;

  /// Raw 'glyf' table data (BE).
  ///
  std::unique_ptr<uint8_t[]> glyf;
};

} // ns

class Font::Impl {
 public:
  Impl(const std::string& pathname) {
    std::ifstream ifs(pathname);
    if (!ifs) std::abort();
    SFNT sfnt(ifs);
    sfnt.scan(L'ǅ');
  }
};

Font::Font(const std::string& pathname) : impl(new Impl{pathname}) {}

Font::~Font() {}

void Font::printInfo() {
  // TODO
}
