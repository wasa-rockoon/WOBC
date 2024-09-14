#include "can.h"

namespace driver {

CAN* CAN_instance = nullptr;

bool CAN::begin(unsigned baudrate, pin_t rx, pin_t tx) {
  CAN_instance = this;
  can2040_setup(&cbus, 1);
  can2040_callback_config(&cbus, callback);
  irq_set_exclusive_handler(PIO1_IRQ_0_IRQn, PIOx_IRQHandler);
  NVIC_SetPriority(PIO1_IRQ_0_IRQn, 1);
  NVIC_EnableIRQ(PIO1_IRQ_0_IRQn);

  can2040_start(&cbus, configCPU_CLOCK_HZ, baudrate, rx, tx);
  return true;
}

bool CAN::send(const Frame& frame) {
  struct can2040_stats stats;
  can2040_get_statistics(&cbus, &stats);
  // Serial.printf("\nCAN %d %d %d %d %d\n", stats.rx_total, stats.tx_total, stats.tx_attempt, stats.parse_error, can2040_check_transmit(&cbus));

  can2040_msg msg;
  msg.id             = frame.id;
  if (frame.extended)  msg.id |= CAN2040_ID_EFF;
  if (frame.rtr)       msg.id |= CAN2040_ID_RTR;
  msg.dlc            = frame.length;
  memcpy(msg.data, frame.data, frame.length);
  int retry = 5;
  while (!can2040_check_transmit(&cbus) && retry > 0) {
    vTaskDelay( 1 / portTICK_PERIOD_MS);
    retry--;
  } 
  return can2040_transmit(&cbus, &msg) == 0;
}

void CAN::callback(struct can2040 *cd, uint32_t notify, struct can2040_msg *msg) {
  // Serial.printf("\ncan %d %d\n", msg->id, msg->dlc);
  if (notify == CAN2040_NOTIFY_RX) {
    Frame frame;
    frame.id       = msg->id & ~(CAN2040_ID_EFF | CAN2040_ID_RTR);
    frame.extended = bool(msg->id & CAN2040_ID_EFF);
    frame.rtr      = bool(msg->id & CAN2040_ID_RTR);
    frame.length   = msg->dlc;
    memcpy(frame.data, msg->data, frame.length);
    if (CAN_instance != nullptr) CAN_instance->receiver_.onReceive(frame);
  }
  else if (notify == CAN2040_NOTIFY_ERROR) {
    if (CAN_instance != nullptr) CAN_instance->receiver_.onError();
  }
}

void CAN::PIOx_IRQHandler(void) {
  can2040_pio_irq_handler(&CAN_instance->cbus);
}

};