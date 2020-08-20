/*
 * cc1101_heater.cpp
 * Copyright (c) 2020 Jarno Kyttälä
 * 
 * ---------------------------------
 * 
 * Simple class for Arduino to control an inexpensive Chinese diesel 
 * heater through 443 MHz RF by using a TI CC1101 transceiver.
 * Replicates the protocol used by the four button "red LCD remote" with
 * an OLED screen, and should probably work if your heater supports this 
 * type of remote controller.
 * 
 * Happy hacking!
 * 
 */

#include <SPI.h>
#include <Arduino.h>
#include "cc1101_heater.h"

void CC1101_Heater::begin(uint32_t heaterAddr) {

  pinMode(_pinSck, OUTPUT);
  pinMode(_pinMosi, OUTPUT);
  pinMode(_pinMiso, INPUT);
  pinMode(_pinSs, OUTPUT);
  pinMode(_pinGdo2, INPUT);
  
  SPI.begin(_pinSck, _pinMosi, _pinMiso, _pinSs);

  _heaterAddr = heaterAddr;

  delay(100);

  initRadio();

}

bool CC1101_Heater::getState(uint8_t *state, uint8_t *power, float *voltage, int8_t *ambientTemp, int8_t *caseTemp, int8_t *setpoint, float *pumpFreq, uint8_t *autoMode, int16_t *rssi, uint32_t timeout) {

  unsigned long t = millis();
  uint8_t rxLen;

  rxFlush();
  rxEnable();

  while (1) {

    if (millis() - t > timeout) return false;

    // Wait for GDO2 assertion
    while (!digitalRead(_pinGdo2)) { if (millis() - t > timeout) return false; }
  
    // Get number of bytes in RX FIFO
    rxLen = writeReg(0xFB, 0xFF);

    if (rxLen == 24) break;

    // Flush RX FIFO
    rxFlush();
    rxEnable();

  }

  // Read RX FIFO
  char buf[64];
  rx(rxLen, buf); 

  /*
  Serial.printf("Received %d bytes\n", rxLen);
  for (int i = 0; i < rxLen; i++) {
     Serial.print(rx[i], HEX);
     Serial.print(" ");     
  }
  Serial.println();
  */
  
  rxFlush();

  uint16_t crc = crc16_2(buf, 19);
  if (crc == (buf[19] << 8) + buf[20]) {
    *state = buf[6];
    *power = buf[7];
    *voltage = buf[9] / 10.0f;
    *ambientTemp = buf[10];
    *caseTemp = buf[12];
    *setpoint = buf[13];
    *autoMode = buf[14] == 0x32; // 0x32 = auto (thermostat), 0xCD = manual (Hertz mode) 
    *pumpFreq = buf[15] / 10.0f;
    *rssi = (buf[22] - (buf[22] >= 128 ? 256 : 0)) / 2 - 74;
    return true;
  }

  return false;

}

void CC1101_Heater::sendCommand(uint8_t cmd, uint32_t addr, uint8_t numTransmits) {

  char buf[10];

  buf[0] = 9; // Packet length, excl. self
  buf[1] = cmd;
  buf[2] = (addr >> 24) & 0xFF;
  buf[3] = (addr >> 16) & 0xFF;
  buf[4] = (addr >> 8) & 0xFF;
  buf[5] = addr & 0xFF;
  buf[6] = _packetSeq++;
  buf[9] = 0;

  uint16_t crc = crc16_2(buf, 7);

  buf[7] = (crc >> 8) & 0xFF;
  buf[8] = crc & 0xFF;

  for (int i = 0; i < numTransmits; i++) {
    txBurst(10, buf);
    delay(16);
  }

  /*
    while (cc1101_writeReg(0xF5, 0xFF) != 0x01) { yield(); } // Wait for idle state
  */

}

void CC1101_Heater::initRadio() {

  writeStrobe(0x30); // SRES

  delay(100);

  writeReg(0x00, 0x07);
  writeReg(0x02, 0x06);
  writeReg(0x03, 0x47);
  writeReg(0x07, 0x04);
  writeReg(0x08, 0x05);
  writeReg(0x0A, 0x00);
  writeReg(0x0B, 0x06);
  writeReg(0x0C, 0x00);
  writeReg(0x0D, 0x10);
  writeReg(0x0E, 0xB1);
  writeReg(0x0F, 0x3B);
  writeReg(0x10, 0xF8);
  writeReg(0x11, 0x93);
  writeReg(0x12, 0x13);
  writeReg(0x13, 0x22);
  writeReg(0x14, 0xF8);
  writeReg(0x15, 0x26);
  writeReg(0x17, 0x30);
  writeReg(0x18, 0x18);
  writeReg(0x19, 0x16);
  writeReg(0x1A, 0x6C);
  writeReg(0x1B, 0x03);
  writeReg(0x1C, 0x40);
  writeReg(0x1D, 0x91);
  writeReg(0x20, 0xFB);
  writeReg(0x21, 0x56);
  writeReg(0x22, 0x17);
  writeReg(0x23, 0xE9);
  writeReg(0x24, 0x2A);
  writeReg(0x25, 0x00);
  writeReg(0x26, 0x1F);
  writeReg(0x2C, 0x81);
  writeReg(0x2D, 0x35);
  writeReg(0x2E, 0x09);
  writeReg(0x09, 0x00);
  writeReg(0x04, 0x7E);
  writeReg(0x05, 0x3C);

  char patable[8] = {0x00, 0x12, 0x0E, 0x34, 0x60, 0xC5, 0xC1, 0xC0};
  writeBurst(0x7E, 8, patable); // PATABLE

  writeStrobe(0x31); // SFSTXON  
  writeStrobe(0x36); // SIDLE  
  writeStrobe(0x3B); // SFTX  
  writeStrobe(0x36); // SIDLE  
  writeStrobe(0x3A); // SFRX  

  delay(136);  
 
}

void CC1101_Heater::txBurst(uint8_t len, char *bytes) {
    txFlush();
    //cc1101_writeReg(0x3F, len);
    writeBurst(0x7F, len, bytes);
    writeStrobe(0x35); // STX
    delay(16); 
}

void CC1101_Heater::txFlush() {
  writeStrobe(0x36); // SIDLE
  writeStrobe(0x3B); // SFTX
  delay(16); // Needed to prevent TX underflow if bursting right after flushing
}

void CC1101_Heater::rx(uint8_t len, char *bytes) {
  for (int i = 0; i < len; i++) {
    bytes[i] = writeReg(0xBF, 0xFF);
  }
}

void CC1101_Heater::rxFlush() {
  writeStrobe(0x36); // SIDLE  
  writeReg(0xBF, 0xFF); // Dummy read to de-assert GDO2
  writeStrobe(0x3A); // SFRX
  delay(16);
}

void CC1101_Heater::rxEnable() {
  writeStrobe(0x34); // SRX  
}

uint8_t CC1101_Heater::writeReg(uint8_t addr, uint8_t val) {
  spiStart();
  SPI.transfer(addr);
  uint8_t tmp = SPI.transfer(val); 
  spiEnd();  
  return tmp;
}

void CC1101_Heater::writeBurst(uint8_t addr, uint8_t len, char *bytes) {
  spiStart();
  SPI.transfer(addr);
  for (int i = 0; i < len; i++) {
    SPI.transfer(bytes[i]);
  }
  spiEnd();
}

void CC1101_Heater::writeStrobe(uint8_t addr) {
  spiStart();
  SPI.transfer(addr);
  spiEnd();
}

void CC1101_Heater::spiStart() {
  digitalWrite(_pinSs, LOW);
  while(digitalRead(_pinMiso));  
}

void CC1101_Heater::spiEnd() {
  digitalWrite(_pinSs, HIGH); 
}

/*
 * CRC-16/MODBUS
 */
uint16_t CC1101_Heater::crc16_2(char *buf, int len) {

  uint16_t crc = 0xFFFF;

  for (int pos = 0; pos < len; pos++) {
    crc ^= (byte)buf[pos];
    for (int i = 8; i != 0; i--) {    
      if ((crc & 0x0001) != 0) {      
        crc >>= 1;                    
        crc ^= 0xA001;
      } else {                    
        crc >>= 1;
      }
    }
  }
  return crc;
}