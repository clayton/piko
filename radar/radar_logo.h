#pragma once

#include <initializer_list>
#include <stdint.h>
#include <string.h>

inline bool radarLogoKnown(const char *code) {
  return code && (!strcmp(code, "AAL") || !strcmp(code, "SWA") || !strcmp(code, "DAL") ||
                  !strcmp(code, "UAL") || !strcmp(code, "JBU") || !strcmp(code, "ASA") ||
                  !strcmp(code, "FFT") || !strcmp(code, "SKW") || !strcmp(code, "ASI") ||
                  !strcmp(code, "SCA"));
}

inline bool radarLogoCode(char output[4], const char *enrichedCode, const char *callsign) {
  for (const char *candidate : {enrichedCode, callsign}) {
    if (!candidate) continue;
    char code[4] = {candidate[0], candidate[1], candidate[2], '\0'};
    if (radarLogoKnown(code)) {
      memcpy(output, code, sizeof(code));
      return true;
    }
  }
  output[0] = '\0';
  return false;
}

template <typename Canvas>
void drawRadarLogo(Canvas &canvas, const char *code, int x, int y) {
  if (!radarLogoKnown(code)) return;
  canvas.fillRoundRect(x, y, 76, 76, 10, 0x1082);

  if (!strcmp(code, "SWA")) {
    canvas.fillTriangle(x + 12, y + 18, x + 38, y + 62, x + 38, y + 31, 0xF800);
    canvas.fillTriangle(x + 64, y + 18, x + 38, y + 62, x + 38, y + 31, 0x001F);
    canvas.fillRect(x + 21, y + 22, 34, 8, 0xFFE0);
  } else if (!strcmp(code, "JBU")) {
    const uint16_t blues[] = {0x0012, 0x021F, 0x041F, 0x07FF};
    for (int row = 0; row < 5; ++row)
      for (int col = 0; col < 5; ++col)
        canvas.fillRect(x + 8 + col * 12, y + 8 + row * 12, 10, 10, blues[(row + col * 3) % 4]);
  } else if (!strcmp(code, "AAL")) {
    for (int i = 0; i < 5; ++i)
      canvas.fillRect(x + 13 + i * 7, y + 13 + i * 9, 48 - i * 6, 7, i < 2 ? 0x001F : 0xF800);
  } else if (!strcmp(code, "DAL")) {
    canvas.fillTriangle(x + 38, y + 9, x + 12, y + 64, x + 64, y + 64, 0xF800);
    canvas.fillTriangle(x + 38, y + 27, x + 25, y + 58, x + 51, y + 58, 0x0012);
  } else if (!strcmp(code, "UAL")) {
    canvas.drawCircle(x + 38, y + 38, 28, 0x07FF);
    canvas.drawCircle(x + 38, y + 38, 15, 0x07FF);
    canvas.drawFastHLine(x + 11, y + 28, 55, 0x07FF);
    canvas.drawFastHLine(x + 11, y + 48, 55, 0x07FF);
    canvas.drawFastVLine(x + 38, y + 10, 57, 0x07FF);
  } else if (!strcmp(code, "ASA")) {
    canvas.fillTriangle(x + 8, y + 62, x + 30, y + 22, x + 44, y + 62, 0x07FF);
    canvas.fillTriangle(x + 28, y + 62, x + 51, y + 12, x + 69, y + 62, 0xFFFF);
  } else if (!strcmp(code, "FFT")) {
    canvas.fillTriangle(x + 9, y + 62, x + 48, y + 10, x + 68, y + 62, 0x07E0);
    canvas.fillTriangle(x + 29, y + 62, x + 49, y + 34, x + 58, y + 62, 0xFFE0);
  } else if (!strcmp(code, "SKW")) {
    canvas.setTextSize(3);
    canvas.setTextColor(0xFFFF);
    canvas.setCursor(x + 10, y + 28);
    canvas.print("SKY");
    canvas.drawFastHLine(x + 9, y + 57, 58, 0x0191);
  } else if (!strcmp(code, "ASI")) {
    canvas.fillTriangle(x + 38, y + 9, x + 9, y + 62, x + 24, y + 62, 0x07FF);
    canvas.fillTriangle(x + 38, y + 9, x + 52, y + 62, x + 67, y + 62, 0x001F);
    canvas.fillRect(x + 24, y + 46, 28, 7, 0xFFFF);
  } else if (!strcmp(code, "SCA")) {
    canvas.drawCircle(x + 38, y + 38, 25, 0xFFE0);
    canvas.setTextSize(3);
    canvas.setTextColor(0x07FF);
    canvas.setCursor(x + 11, y + 28);
    canvas.print("SC");
  }
}
