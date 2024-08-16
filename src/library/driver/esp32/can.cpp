#include "can.h"

namespace driver {

CAN* CAN_instance = nullptr;

bool CAN::begin(unsigned baudrate, pin_t rx, pin_t tx) {
  CAN_instance = this;
  CAN0.setCANPins((gpio_num_t)rx, (gpio_num_t)tx);
  if (!CAN0.begin(baudrate)) ;
  // return false;
  // CAN0.setRXFilter(0, 0, true);
  CAN0.watchFor();
  CAN0.setCallback(0, &callback);
  return true;
}

bool CAN::send(const Frame& frame) {
  CAN_FRAME can_frame;
  can_frame.id       = frame.id;
  can_frame.extended = frame.extended;
  can_frame.rtr      = frame.rtr;
  can_frame.length   = frame.length;
  memcpy(can_frame.data.uint8, frame.data, frame.length);
  return CAN0.sendFrame(can_frame);
}

void CAN::update() {
  CAN_FRAME can_frame;
  if (CAN0.read(can_frame)) {
    callback(&can_frame);
    // printf("READ %d", can_frame.length);
  }
}

void CAN::callback(CAN_FRAME *can_frame) {
  Frame frame;
  frame.id       = can_frame->id;
  frame.extended = can_frame->extended;
  frame.rtr      = can_frame->rtr;
  frame.length   = can_frame->length;
  memcpy(frame.data, can_frame->data.uint8, frame.length);
  if (CAN_instance != nullptr) CAN_instance->receiver_.onReceive(frame);
}

};