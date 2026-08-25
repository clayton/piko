#pragma once

#include <Arduino.h>

void copyText(char *destination, size_t size, const char *source);
void centered(const char *text, int y, int size, uint16_t color);
