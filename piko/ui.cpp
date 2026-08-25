#include "ui.h"

#include "hw.h"

void copyText(char *destination, size_t size, const char *source) {
  snprintf(destination, size, "%s", source ? source : "");
  for (int i = strlen(destination) - 1; i >= 0 && destination[i] == ' '; --i) destination[i] = '\0';
}

void centered(const char *text, int y, int size, uint16_t color) {
  canvas.setTextSize(size);
  canvas.setTextColor(color);
  int width = strlen(text) * 6 * size;
  canvas.setCursor(max(8, (LCD_WIDTH - width) / 2), y);
  canvas.print(text);
}
