/*
  uart_echo_demo.ino
  Simple bidirectional UART passthrough demo over ESP-NOW.

  Tested on Seeed XIAO ESP32-C3.
  Edit UART_PORT / UART_HAS_PINS / UART_RX_PIN / UART_TX_PIN as needed.
  Set peerMac to the target device MAC, or use FF:FF:FF:FF:FF:FF for broadcast.

  Local commands: /help, /mac, /stat  
*/
#include <Arduino.h>
#include "espnow_uart_bridge.h"

#define LED_PIN 2
#define UART_BAUD 115200
#define UART_PORT Serial
// #define UART_PORT Serial1

// #define D7 20
// #define D6 21
// #define UART_RX_PIN D7
// #define UART_TX_PIN D6

// Set the parent MAC address here.
// To use broadcast, set peerMac to FF:FF:FF:FF:FF:FF.
const uint8_t peerMac[6] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };

void setup() {
  espnow_uart_bridge::configure(peerMac, UART_PORT, LED_PIN);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  UART_PORT.begin(UART_BAUD);
  // UART_PORT.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  delay(200);

  espnow_uart_bridge::initializeEspNow();
}

void loop() {
  espnow_uart_bridge::processUartLoop();
  delay(1);
}
