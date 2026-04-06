// src/library/driver/rp2350/can.cpp
#include "can.h"

// RP2350 用ハードウェアヘッダ
#include <hardware/irq.h>
#include <hardware/gpio.h>

namespace driver {

CAN* CAN::instance_ = nullptr;

bool CAN::begin(unsigned baudrate, pin_t rx, pin_t tx) {
  instance_ = this;
  can2040_setup(&cbus, 1);
  can2040_callback_config(&cbus, callback);
  
  // RP2350 での割り込み設定（RP2040 と同じ）
  irq_set_exclusive_handler(PIO1_IRQ_0, PIOx_IRQHandler);
  irq_set_enabled(PIO1_IRQ_0, true);

  can2040_start(&cbus, configCPU_CLOCK_HZ, baudrate, rx, tx);
  return true;
}

bool CAN::send(const Frame& frame) {
  can2040_msg msg;
  msg.id             = frame.id;
  if (frame.extended)  msg.id |= CAN2040_ID_EFF;
  if (frame.rtr)       msg.id |= CAN2040_ID_RTR;
  msg.dlc            = frame.length;
  memcpy(msg.data, frame.data, frame.length);
  
  int retry = 5;
  while (!can2040_check_transmit(&cbus) && retry > 0) {
    vTaskDelay(1 / portTICK_PERIOD_MS);
    retry--;
  }
  
  return can2040_transmit(&cbus, &msg) == 0;
}

void CAN::callback(struct can2040 *cd, uint32_t notify, struct can2040_msg *msg) {
  if (notify == CAN2040_NOTIFY_RX) {
    Frame frame;
    frame.id       = msg->id & ~(CAN2040_ID_EFF | CAN2040_ID_RTR);
    frame.extended = bool(msg->id & CAN2040_ID_EFF);
    frame.rtr      = bool(msg->id & CAN2040_ID_RTR);
    frame.length   = msg->dlc;
    memcpy(frame.data, msg->data, frame.length);
    if (instance_ != nullptr) instance_->receiver_.onReceive(frame);
  }
  else if (notify == CAN2040_NOTIFY_ERROR) {
    if (instance_ != nullptr) instance_->receiver_.onError();
  }
}

void CAN::PIOx_IRQHandler(void) {
  can2040_pio_irq_handler(&instance_->cbus);
}

}