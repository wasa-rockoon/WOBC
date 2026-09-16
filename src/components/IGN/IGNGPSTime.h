#pragma once

#include <stdint.h>
#include <string.h>

namespace component {

// 既存GPSのUT (YYYY-MM-DD hh:mm:ss.cc) を比較可能な整数へ変換する。
// 実時間との差は取らず、同じUTCの再送で受信鮮度を延長しないために使う。
inline bool parseIGNGPSUtc(const char* utc, uint64_t& key) {
  if (!utc || strlen(utc) != 22) return false;
  uint64_t parsed = 0;
  for (unsigned i = 0; i < 22; ++i) {
    char separator = 0;
    if (i == 4 || i == 7) separator = '-';
    else if (i == 10) separator = ' ';
    else if (i == 13 || i == 16) separator = ':';
    else if (i == 19) separator = '.';
    if (separator) {
      if (utc[i] != separator) return false;
    } else {
      if (utc[i] < '0' || utc[i] > '9') return false;
      parsed = parsed * 10 + static_cast<unsigned>(utc[i] - '0');
    }
  }
  const auto twoDigits = [utc](unsigned i) {
    return (utc[i] - '0') * 10 + utc[i + 1] - '0';
  };
  const int year = twoDigits(0) * 100 + twoDigits(2);
  const int month = twoDigits(5);
  const int day = twoDigits(8);
  const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (year < 2000 || month < 1 || month > 12) return false;
  const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  const int max_day = days[month - 1] + (month == 2 && leap ? 1 : 0);
  if (day < 1 || day > max_day || twoDigits(11) > 23
      || twoDigits(14) > 59 || twoDigits(17) > 60) return false;
  key = parsed;
  return true;
}

}  // namespace component
