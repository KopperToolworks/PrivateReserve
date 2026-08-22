#include "lcd.h"

#include "docs_qr.h"
#include "door.h"
#include "store.h"
#include "fonts/FreeSans9pt7b.h"

#include <cstdio>

namespace {
uint16_t rgb(Arduino_GFX* g, uint8_t r, uint8_t gc, uint8_t b) {
  return g->color565(r, gc, b);
}

void text(Arduino_GFX* g, int x, int y, uint16_t c, const char* s) {
  g->setTextSize(1);
  g->setTextColor(c);
  g->setCursor(x, y);
  g->print(s);
}

void text2(Arduino_GFX* g, int x, int y, uint16_t c, const char* s) {
  g->setTextSize(2);
  g->setTextColor(c);
  g->setCursor(x, y);
  g->print(s);
}

void pip(Arduino_GFX* g, int x, int y, uint8_t p, uint16_t ok, uint16_t warn,
         uint16_t bad, uint16_t idle) {
  uint16_t c = idle;
  if (p == 1) {
    c = ok;
  } else if (p == 2) {
    c = warn;
  } else if (p == 3) {
    c = bad;
  }
  g->fillCircle(x, y, 3, c);
}

uint16_t clsColor(Arduino_GFX* g, char cls, uint16_t text, uint16_t ok,
                  uint16_t move, uint16_t fault) {
  if (cls == 'k') {
    return ok;
  }
  if (cls == 'm') {
    return move;
  }
  if (cls == 'f') {
    return fault;
  }
  return text;
}

// Fixed 0–2.50 A window. Hist is centiamps. Left-pads a short series.
void drawAmpStrip(Arduino_GFX* g, int y, float amps, uint16_t textc,
                  uint16_t dim, uint16_t accent, uint16_t fault,
                  uint16_t panel, uint16_t line) {
  char lab[12];
  snprintf(lab, sizeof(lab), "%.2f A", amps);
  text(g, 6, y + 6, textc, lab);
  text(g, 6, y + 15, dim, "0-2.50");
  const int gx0 = 56;
  const int gx1 = 314;
  const int gy0 = y;
  const int gy1 = y + 20;
  g->fillRect(gx0, gy0, gx1 - gx0, gy1 - gy0, panel);
  g->drawRect(gx0, gy0, gx1 - gx0, gy1 - gy0, line);
  g->drawFastHLine(gx0 + 1, gy0 + 1, gx1 - gx0 - 2, fault);
  const uint16_t* h = app.ampHist();
  const uint8_t n = app.histCount();
  if (n < 2) {
    return;
  }
  constexpr int kAmax = 250;
  const int span = gx1 - gx0 - 3;
  int px = -1;
  int py = 0;
  for (uint8_t i = 0; i < n; ++i) {
    int c = static_cast<int>(h[app.histChronoIndex(i)]);
    if (c < 0) {
      c = 0;
    }
    if (c > kAmax) {
      c = kAmax;
    }
    const int x = gx0 + 1 + (kHistLen - n + i) * span / (kHistLen - 1);
    const int yy = gy1 - 2 - c * (gy1 - gy0 - 4) / kAmax;
    if (px >= 0) {
      g->drawLine(px, py, x, yy, accent);
    }
    px = x;
    py = yy;
  }
  g->fillCircle(px, py, 2, accent);
}
}  // namespace

void lcdBegin(Arduino_GFX* canvas) {
  pinMode(kPanelPowerPin, OUTPUT);
  digitalWrite(kPanelPowerPin, HIGH);
  pinMode(kBacklightPin, OUTPUT);
  digitalWrite(kBacklightPin, HIGH);
  (void)canvas;
}

void lcdDraw(Arduino_GFX* g) {
  const View& v = app.view();
  const uint16_t bg = rgb(g, 11, 13, 16);
  const uint16_t panel = rgb(g, 20, 24, 29);
  const uint16_t line = rgb(g, 36, 42, 49);
  const uint16_t textc = rgb(g, 230, 233, 236);
  const uint16_t dim = rgb(g, 138, 147, 156);
  const uint16_t faint = rgb(g, 95, 106, 117);
  const uint16_t ok = rgb(g, 76, 175, 109);
  const uint16_t move = rgb(g, 224, 163, 58);
  const uint16_t fault = rgb(g, 224, 75, 67);
  const uint16_t idle = rgb(g, 91, 101, 112);
  const uint16_t accent = rgb(g, 90, 169, 230);

  g->fillScreen(bg);
  text(g, 6, 3, textc, v.hdr_mode);
  text(g, 200, 3, dim, v.hdr_net);
  g->fillCircle(190, 7, 2, v.net_ap ? move : ok);

  int y = 16;

  auto banner = [&](uint16_t border, uint16_t fill, uint16_t tc, uint16_t dc) {
    g->fillRect(6, y, 308, 36, fill);
    g->drawRect(6, y, 308, 36, border);
    text2(g, 10, y + 4, tc, v.hero_big);
    text(g, 10, y + 24, dc, v.hero_sub);
    y += 40;
  };

  if (v.hero == View::Hero::Bad) {
    banner(fault, rgb(g, 42, 18, 17), rgb(g, 255, 138, 130),
           rgb(g, 217, 162, 158));
  } else if (v.hero == View::Hero::Warn) {
    banner(move, rgb(g, 42, 34, 17), rgb(g, 240, 192, 112),
           rgb(g, 203, 176, 131));
  } else if (v.hero == View::Hero::Ok) {
    banner(ok, rgb(g, 15, 36, 24), rgb(g, 121, 215, 154), rgb(g, 157, 196, 172));
  } else if (v.hero == View::Hero::Action) {
    const uint16_t hc = clsColor(g, v.hero_cls, textc, ok, move, fault);
    text2(g, 6, y, hc, v.hero_big);
    text(g, 6, y + 18, dim, v.hero_sub);
    if (v.hero_right[0]) {
      text(g, 210, y + 4, textc, v.hero_right);
    }
    if (v.hero_right2[0]) {
      text(g, 210, y + 16, faint, v.hero_right2);
    }
    if (v.show_amps) {
      char a[16];
      snprintf(a, sizeof(a), "%.2f A", v.amps);
      text(g, 230, y, textc, a);
      const int bw = 84;
      const int bx = 228;
      const int by = y + 14;
      g->fillRect(bx, by, bw, 4, rgb(g, 30, 36, 43));
      int fillw = static_cast<int>(v.amps / 2.5f * bw);
      if (fillw < 0) {
        fillw = 0;
      }
      if (fillw > bw) {
        fillw = bw;
      }
      g->fillRect(bx, by, fillw, 4, v.amps > v.trip ? fault : ok);
      if (v.trip > 0) {
        const int tx = bx + static_cast<int>(v.trip / 2.5f * bw);
        g->drawFastVLine(tx, by - 2, 8, fault);
      }
      char cap[28];
      snprintf(cap, sizeof(cap), "trip %.2f  max 2.50", v.trip);
      text(g, bx, by + 6, dim, cap);
    }
    if (v.spark) {
      const uint16_t* h = v.spark == 1 ? app.hallHist() : app.ampHist();
      const uint8_t n = app.histCount();
      const int sx = 200;
      const int sy = y;
      if (n > 0) {
        uint16_t mn = 65535, mx = 0;
        for (uint8_t i = 0; i < n; ++i) {
          const uint16_t s = h[app.histChronoIndex(i)];
          if (s < mn) {
            mn = s;
          }
          if (s > mx) {
            mx = s;
          }
        }
        if (mx <= mn) {
          mx = mn + 1;
        }
        int px = -1, py = -1;
        int lastx = sx, lasty = sy;
        for (uint8_t i = 0; i < n; ++i) {
          const uint16_t s = h[app.histChronoIndex(i)];
          const int x = sx + (kHistLen - n + i) * 2;
          const int yy =
              sy + 22 - static_cast<int>((s - mn) * 20 / (mx - mn));
          if (px >= 0) {
            g->drawLine(px, py, x, yy, accent);
          }
          px = x;
          py = yy;
          lastx = x;
          lasty = yy;
        }
        g->fillCircle(lastx, lasty, 2, accent);
      }
    }
    y += 40;
  }

  if (v.show_qr) {
    constexpr int pix = 3;
    const int q = kDocsQrSize * pix;
    const int qx = 8;
    const int qy = y;
    g->fillRect(qx, qy, q, q, rgb(g, 255, 255, 255));
    for (int row = 0; row < kDocsQrSize; ++row) {
      int col = 0;
      while (col < kDocsQrSize) {
        if (!docsQrModule(col, row)) {
          ++col;
          continue;
        }
        const int x0 = col;
        while (col < kDocsQrSize && docsQrModule(col, row)) {
          ++col;
        }
        g->fillRect(qx + x0 * pix, qy + row * pix, (col - x0) * pix, pix,
                    rgb(g, 0, 0, 0));
      }
    }
    const int tx = qx + q + 10;
    text(g, tx, qy + 8, dim, kDocsHost);
    text(g, tx, qy + 20, textc, kDocsRepo);
    text(g, tx, qy + 40, faint, "source and docs");
    text(g, tx, qy + 52, faint, "scan to open");
    y = qy + q + 4;
  }

  if (v.show_entry) {
    g->drawRect(6, y, 90, 28, accent);
    text2(g, 10, y + 6, textc, v.entry_val);
    text(g, 100, y + 6, dim, v.entry_unit);
    text(g, 100, y + 16, faint, v.entry_note);
    y += 32;
  }
  if (v.show_range) {
    const int bw = 308;
    g->fillRect(6, y, bw, 6, rgb(g, 30, 36, 43));
    const int span = v.range_max - v.range_min;
    if (span > 0) {
      int fw = (v.range_val - v.range_min) * bw / span;
      if (fw < 0) {
        fw = 0;
      }
      if (fw > bw) {
        fw = bw;
      }
      g->fillRect(6, y, fw, 6, accent);
      const int dx = 6 + (v.range_def - v.range_min) * bw / span;
      g->drawFastVLine(dx, y - 2, 10, dim);
    }
    y += 10;
  }

  if (v.show_travel) {
    text(g, 6, y + 4, dim, "OPEN");
    g->fillRect(40, y + 2, 240, 14, panel);
    g->drawRect(40, y + 2, 240, 14, line);
    if (v.pos_known) {
      g->fillRect(41, y + 3, v.pos_pct * 238 / 100, 12, rgb(g, 36, 52, 71));
      const int kx = 40 + v.pos_pct * 238 / 100;
      g->fillRect(kx - 1, y + 1, 3, 16, v.moving ? move : (v.fault_knob ? fault : textc));
    }
    g->drawFastVLine(40 + 3 * 238 / 100, y + 2, 14,
                     v.open_mark ? ok : rgb(g, 74, 85, 99));
    g->drawFastVLine(40 + 95 * 238 / 100, y + 2, 14,
                     v.close_mark ? ok : rgb(g, 74, 85, 99));
    text(g, 284, y + 4, dim, "CLOSED");
    char sc[24];
    if (v.pos_known) {
      snprintf(sc, sizeof(sc), "%d%%", v.pos_pct);
    } else {
      snprintf(sc, sizeof(sc), "unknown");
    }
    text(g, 44, y + 18, dim, sc);
    char cc[20];
    snprintf(cc, sizeof(cc), "%ld cnt", static_cast<long>(v.counts));
    text(g, 220, y + 18, dim, v.pos_known ? cc : "-");
    y += 30;
  }

  if (v.spark == 2 && v.hero != View::Hero::Action) {
    drawAmpStrip(g, y, v.amps, textc, dim, accent, fault, panel, line);
    y += 24;
  }

  if (v.graph) {
    const LoadTable* t = store.armedTable();
    const LoadBin* bins =
        t ? (v.graph == 1 ? t->close : t->open) : nullptr;
    const int gx0 = 22, gx1 = 316, gy0 = y + 4, gy1 = y + 56;
    constexpr float amin = 0.7f, amax = 2.6f;
    auto X = [&](int b) {
      return gx0 + b * (gx1 - gx0) / (kBinCount - 1);
    };
    auto Y = [&](float a) {
      float tnorm = (a - amin) / (amax - amin);
      if (tnorm < 0) {
        tnorm = 0;
      }
      if (tnorm > 1) {
        tnorm = 1;
      }
      return gy1 - static_cast<int>(tnorm * (gy1 - gy0));
    };
    g->fillRect(gx0, gy0, gx1 - gx0, gy1 - gy0, rgb(g, 16, 18, 22));
    g->drawFastHLine(gx0, Y(2.5f), gx1 - gx0, fault);
    if (bins) {
      int px = -1, pmean = 0, ptrip = 0;
      for (int b = 0; b < kBinCount; ++b) {
        if (!bins[b].valid) {
          continue;
        }
        const int x = X(b);
        const int ym = Y(bins[b].mean);
        const int yt = Y(bins[b].trip);
        if (px >= 0) {
          g->drawLine(px, pmean, x, ym, accent);
          g->drawLine(px, ptrip, x, yt, fault);
        }
        px = x;
        pmean = ym;
        ptrip = yt;
      }
    }
    const int lx = X(v.live_bin);
    g->drawFastVLine(lx, gy0, gy1 - gy0, move);
    text(g, gx0, gy1 + 1, faint, "OPEN");
    text(g, gx1 - 36, gy1 + 1, faint, "CLOSED");
    y = gy1 + 12;
  }

  if (v.ntiles) {
    const int cols = v.tile_cols ? v.tile_cols : 4;
    const int tw = (308 - (cols - 1) * 4) / cols;
    for (uint8_t i = 0; i < v.ntiles; ++i) {
      const int x = 6 + (i % cols) * (tw + 4);
      const int ty = y + (i / cols) * 40;
      g->fillRect(x, ty, tw, 38, panel);
      g->drawRect(x, ty, tw, 38, v.tiles[i].sel ? accent : line);
      text(g, x + 4, ty + 3, dim, v.tiles[i].k);
      pip(g, x + 8, ty + 18, v.tiles[i].pip, ok, move, fault, idle);
      text(g, x + 16, ty + 14, textc, v.tiles[i].v);
      text(g, x + 4, ty + 26, faint, v.tiles[i].g);
    }
    y += ((v.ntiles + cols - 1) / cols) * 40 + 2;
  }

  if (v.nitems) {
    g->setFont(&FreeSans9pt7b);
    g->setTextSize(1);
    for (uint8_t i = 0; i < v.nitems; ++i) {
      const int iy = y + i * kMenuRowPx;
      if (v.items[i].sel) {
        g->fillRect(6, iy, 308, kMenuRowPx, rgb(g, 29, 39, 51));
        g->fillRect(6, iy, 2, kMenuRowPx, accent);
      }
      if (v.items[i].pip || v.items[i].label[0]) {
        pip(g, 14, iy + 8, v.items[i].pip, ok, move, fault, idle);
      }
      text(g, 22, iy + 12, textc, v.items[i].label);
      int16_t x1 = 0, y1 = 0;
      uint16_t tw = 0, th = 0;
      g->getTextBounds(v.items[i].value, 0, 0, &x1, &y1, &tw, &th);
      text(g, 314 - static_cast<int>(tw), iy + 12, faint, v.items[i].value);
    }
    g->setFont(nullptr);
    g->setTextSize(1);
    y += v.nitems * kMenuRowPx + 2;
  }

  for (uint8_t i = 0; i < v.nrows; ++i) {
    if (v.rows[i].sel) {
      g->fillRect(6, y - 1, 308, 11, rgb(g, 29, 39, 51));
      g->fillRect(6, y - 1, 2, 11, accent);
    }
    text(g, 10, y, dim, v.rows[i].k);
    const int vw = strlen(v.rows[i].v) * 6;
    text(g, 314 - vw, y, v.rows[i].dim ? faint : textc, v.rows[i].v);
    y += 11;
  }

  if (v.show_prog) {
    g->fillRect(6, y, 308, 8, rgb(g, 30, 36, 43));
    const int sw = 74;
    for (int i = 0; i < 4; ++i) {
      uint16_t c = rgb(g, 43, 51, 60);
      if (v.prog[i] == 1) {
        c = ok;
      } else if (v.prog[i] == 2) {
        c = move;
      }
      g->fillRect(8 + i * (sw + 2), y + 1, sw, 6, c);
    }
    text(g, 6, y + 10, dim, v.prog_l);
    text(g, 260, y + 10, dim, v.prog_r);
    y += 20;
  }

  if (v.show_pin) {
    g->fillRect(6, 144, 308, 13, rgb(g, 16, 20, 24));
    g->drawRect(6, 144, 308, 13, line);
    text(g, 8, 147, dim, v.pin_parts);
    const int vw = strlen(v.pin_lv) * 6;
    text(g, 312 - vw, 147, textc, v.pin_lv);
  }

  int fx = 6;
  for (uint8_t i = 0; i < v.nftr; ++i) {
    text(g, fx, 159, faint, v.ftr[i]);
    fx += strlen(v.ftr[i]) * 6 + 12;
  }
}
