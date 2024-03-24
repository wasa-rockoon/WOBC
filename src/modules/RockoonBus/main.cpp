#include <FreeRTOS.h>
#include <library/wobc.h>
// #include <packet.h>
#include <components/components.h>


class RockoonBus: public process::Module {
public:

  RockoonBus() : process::Module("RockoonBus") {}

  class AListener: public process::Listener {

  };

  void setup() {
    // start(sd_logger);
  }
  // component::core::SDLogger sd_logger;
};

RockoonBus module;

void setup() {
  // put your setup code here, to run once:

  module.main();
}

void loop() {
  // put your main code here, to run repeatedly:

}

