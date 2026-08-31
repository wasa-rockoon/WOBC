#pragma once

// LiPoPower and PowerMeasure use the same INA226 driver. Keep a single class
// definition so both component headers can be included in one translation unit.
#include <components/LiPoPower/INA226.h>
