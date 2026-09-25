#pragma once

#include <library/process/component.h>
#include "SeparationAckProtocol.h"

namespace component {

class SeparationAck : public process::Component {
public:
    SeparationAck() : process::Component("SeparationAck", SeparationAckProtocol::component_id) {
        priority_ = 1;
    }

protected:
    void onCommand(const wcpp::Packet& command) override {
        if (kernel::unit_id() != SeparationAckProtocol::separation_unit_id
            || !SeparationAckProtocol::accepts(command)) return;
        auto ack = newPacket(32);
        if (protocol_.reply(command, ack)) sendPacket(ack);
    }

private:
    SeparationAckProtocol protocol_;
};

}  // namespace component
