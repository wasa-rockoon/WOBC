#pragma once

#include "IGNGPSTime.h"
#include <library/wcpp/cpp/packet.h>

namespace component {

// MissionBusの既存AL/UT形式のみを使用する。新しいフィールドは要求しない。
inline bool decodeIGNGPS(const wcpp::Packet& packet, int64_t& altitude_m,
                         uint64_t& utc_key) {
  const auto altitude = packet.find("AL");
  const auto utc = packet.find("UT");
  char utc_text[24] = {};
  // 長い文字列のsize()は長さバイトも含む。固定長を確認してからコピーする。
  if (!altitude || !(*altitude).isInt() || !utc || !(*utc).isBytes()
      || (*utc).size() != 23 || (*utc).getString(utc_text) != 22
      || !parseIGNGPSUtc(utc_text, utc_key)) return false;
  altitude_m = (*altitude).getInt();
  return true;
}

}  // namespace component
