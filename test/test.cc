//
// Font
// test.cc
//
// Copyright (C) 2020 Gustavo C. Viegas.
//

#include <iostream>
#include <vector>
#include <cassert>

#include <yf/yf.h>

#include "font.h"

void draw(const Glyph& glyph) {
  const auto gw = glyph.extent().first;
  const auto gh = glyph.extent().second;

  std::wcout << "\n\n~~Draw~~\n\n" <<
    "glyph extent is " << gw << "x" << gh << "\n\n";

  YF_context ctx = yf_context_init();
  assert(ctx);

  YF_modid vmod, fmod;
  if (yf_loadmod(ctx, getenv("VMOD"), &vmod) != 0 ||
    yf_loadmod(ctx, getenv("FMOD"), &fmod) != 0)
  {
    assert(0);
  }
  const YF_stage stgs[] = {
    {YF_STAGE_VERT, vmod, "main"},
    {YF_STAGE_FRAG, fmod, "main"}
  };

  struct Vertex { float pos[3], tc[2]; };

  const YF_vattr attrs[] = {
    {0, YF_TYPEFMT_FLOAT3, 0},
    {1, YF_TYPEFMT_FLOAT2, offsetof(Vertex, tc)}
  };
  const YF_vinput vin = {
    attrs,
    sizeof attrs / sizeof attrs[0],
    sizeof(Vertex),
    YF_VRATE_VERT
  };

  const Vertex verts[] = {
    {{-1.0f, -1.0f, 0.5f}, {0.0f, 1.0f}},
    {{-1.0f, +1.0f, 0.5f}, {0.0f, 0.0f}},
    {{+1.0f, +1.0f, 0.5f}, {1.0f, 0.0f}},
    {{+1.0f, -1.0f, 0.5f}, {1.0f, 1.0f}}
  };
  const unsigned short inds[] = {0, 1, 2, 0, 2, 3};

  YF_buffer buf = yf_buffer_init(ctx, 1<<16);
  assert(buf);
  if (yf_buffer_copy(buf, 0, verts, sizeof verts) != 0 ||
    yf_buffer_copy(buf, sizeof verts, inds, sizeof inds) != 0)
  {
    assert(0);
  }

  YF_image img = yf_image_init(ctx, YF_PIXFMT_R8UNORM, {gw, gh, 1}, 1, 1, 1);
  assert(img);
  if (yf_image_copy(img, {0, 1}, glyph.data(), gw*gh) != 0)
    assert(0);

  const YF_dentry entry = {2, YF_DTYPE_ISAMPLER, 1, nullptr};
  YF_dtable dtb = yf_dtable_init(ctx, &entry, 1);
  assert(dtb);
  if (yf_dtable_alloc(dtb, 1) != 0)
    assert(0);
  const unsigned layer = 0;
  if (yf_dtable_copyimg(dtb, 0, 2, {0, 1}, &img, &layer) != 0)
    assert(0);

  const YF_dim2 winDim = {gw*20U, gh*20U};
  YF_window win = yf_window_init(ctx, winDim, "Font");
  assert(win);

  const YF_colordsc* dsc = yf_window_getdsc(win);
  unsigned attN;
  const YF_attach* atts = yf_window_getatts(win, &attN);
  YF_pass pass = yf_pass_init(ctx, dsc, 1, nullptr, nullptr);
  assert(pass);
  std::vector<YF_target> tgts;
  for (unsigned i = 0; i < attN; ++i) {
    auto t = yf_pass_maketarget(pass, winDim, 1, &atts[i], 1, nullptr, nullptr);
    assert(t);
    tgts.push_back(t);
  }

  const YF_gconf conf = {
    pass,
    stgs,
    sizeof stgs / sizeof stgs[0],
    &dtb,
    1,
    &vin,
    1,
    YF_PRIMITIVE_TRIANGLE,
    YF_POLYMODE_FILL,
    YF_CULLMODE_BACK,
    YF_FRONTFACE_CCW
  };
  YF_gstate gst = yf_gstate_init(ctx, &conf);
  assert(gst);

  int next;
  YF_cmdbuf cb;
  YF_viewport vp;
  YF_VIEWPORT_FROMDIM2(winDim, vp);
  YF_rect sciss;
  YF_VIEWPORT_SCISSOR(vp, sciss);

  for (;;) {
    next = yf_window_next(win);
    assert(next >= 0);

    cb = yf_cmdbuf_begin(ctx, YF_CMDB_GRAPH);
    assert(cb);
    yf_cmdbuf_setgstate(cb, gst);
    yf_cmdbuf_settarget(cb, tgts[next]);
    yf_cmdbuf_setvport(cb, 0, &vp);
    yf_cmdbuf_setsciss(cb, 0, sciss);
    yf_cmdbuf_setdtable(cb, 0, 0);
    yf_cmdbuf_setvbuf(cb, 0, buf, 0);
    yf_cmdbuf_setibuf(cb, buf, sizeof verts, sizeof inds[0]);
    yf_cmdbuf_clearcolor(cb, 0, {1.0f, 1.0f, 1.0f, 1.0f});
    yf_cmdbuf_draw(cb, 1, 0, sizeof inds / sizeof inds[0], 1, 0, 0);
    if (yf_cmdbuf_end(cb) != 0 || yf_cmdbuf_exec(ctx) != 0)
      assert(0);

    if (yf_window_present(win) != 0)
      assert(0);
  }
}

int main(int argc, char* argv[]) {
  std::wcout << "[Font] test\n\n";
  for (int i = 0; i < argc; ++i)
    std::wcout << argv[i] << " ";
  std::wcout << "\n\n";

  const wchar_t chr = argc > 1 ? argv[1][0] : L'+';

  try {
    Font font{std::getenv("FONT")};
    auto glyph = font.getGlyph(chr, 34);
    draw(*glyph);
  } catch (...) {
    std::wcerr << "ERR: 'FONT' env. not defined\n";
    return -1;
  }

  std::wcout << "\n-- end of test --" << std::endl;
  return 0;
}
