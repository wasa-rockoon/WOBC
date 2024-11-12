#include "library/driver/generic_serial.h"

namespace driver {

// arduino-picoのSerialPIOを9600bpsで使ったときにクロックがずれる問題の対処
template <>
void GenericSerial<SerialPIO>::begin(unsigned baud) { 
  if (baud == 9600) baud = 9800; 
  serial_.begin(baud); 
}

}