/* https://github.com/DeviationTX/deviation/blob/master/doc/CX10Blue.txt */
#define CX10_GREEN_PACKET_LENGTH 15
#define CX10_BLUE_PACKET_LENGTH 19
#define CX10_BLUE_PACKET_PERIOD 8000
#define CX10_GREEN_PACKET_PERIOD 3000
#define CX10_GREEN_BIND_COUNT 1000
#define CX10_RF_BIND_CHANNEL 0x02
#define CX10_NUM_RF_CHANNELS 4

static uint8_t CX10_txid[4];                                    // transmitter ID
static const uint8_t CX10_rxid[] = { 0x01, 0x02, 0x03, 0x04 };  // receiver ID
static uint8_t CX10_freq[4];                                    // frequency hopping table
static uint8_t CX10_current_chan = 0;
static uint8_t CX10_packet_length;
static uint32_t CX10_packet_period;
static const uint8_t CX10_tx_rx_id[] = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };
bool bound = false;
bool acknowledged = false;

void CX10_Write_Packet(uint8_t init) {
  uint8_t offset = 0;
  if (current_protocol == PROTO_CX10_BLUE) { offset = 4; }
  packet[0] = init;
  // packet[5] to [8] (aircraft id) is filled during bind for blue board CX10
  packet[5] = CX10_rxid[0];
  packet[6] = CX10_rxid[1];
  packet[7] = CX10_rxid[2];
  packet[8] = CX10_rxid[3];
  if (init == 0x55) {
    packet[5 + offset] = lowByte(3000 - ppm[AILERON]);
    packet[6 + offset] = highByte(3000 - ppm[AILERON]);
    packet[7 + offset] = lowByte(3000 - ppm[ELEVATOR]);
    packet[8 + offset] = highByte(3000 - ppm[ELEVATOR]);
    packet[9 + offset] = lowByte(ppm[THROTTLE]);
    packet[10 + offset] = highByte(ppm[THROTTLE]);
    packet[11 + offset] = lowByte(ppm[RUDDER]);
    packet[12 + offset] = highByte(ppm[RUDDER]);
    if (ppm[AUX2] > PPM_MAX_COMMAND)
      packet[12 + offset] |= 0x10;  // flip flag
    // rate / mode
    if (ppm[AUX1] > PPM_MAX_COMMAND)  // mode 3 / headless on CX-10A
      packet[13 + offset] = 0x02;
    else if (ppm[AUX1] < PPM_MIN_COMMAND)  // mode 1
      packet[13 + offset] = 0x00;
    else  // mode 2
      packet[13 + offset] = 0x01;
    packet[14 + offset] = 0x00;
    if (current_protocol == PROTO_CX10_BLUE) {
      // snapshot (CX10-C)
      if (ppm[AUX3] < PPM_MAX_COMMAND)
        packet[13 + offset] |= 0x10;
      // video recording (CX10-C)
      if (ppm[AUX4] > PPM_MAX_COMMAND)
        packet[13 + offset] |= 0x08;
    }
  }

  XN297_WritePayload(packet, CX10_packet_length);
}

void toBits(char* buffer, uint32_t byte) {
  sprintf(buffer,
          "%c%c%c%c%c%c%c%c",
          (byte & 0x80) ? '1' : '0',
          (byte & 0x40) ? '1' : '0',
          (byte & 0x20) ? '1' : '0',
          (byte & 0x10) ? '1' : '0',
          (byte & 0x08) ? '1' : '0',
          (byte & 0x04) ? '1' : '0',
          (byte & 0x02) ? '1' : '0',
          (byte & 0x01) ? '1' : '0');
}

uint32_t CX10RX_process() {

  uint32_t nextPacket = micros() + CX10_packet_period;
  if (bound) {
    NRF24L01_WriteReg(NRF24L01_05_RF_CH, CX10_freq[CX10_current_chan++]);
    CX10_current_chan = CX10_current_chan % CX10_NUM_RF_CHANNELS;

    NRF24L01_WriteReg(NRF24L01_07_STATUS, 0x70);
    NRF24L01_FlushRx();
    XN297_Configure(_BV(NRF24L01_00_EN_CRC) | _BV(NRF24L01_00_CRCO) | _BV(NRF24L01_00_PWR_UP) | _BV(NRF24L01_00_PRIM_RX));
    uint32_t timeout;
    timeout = millis() + 5;
    while (millis() < timeout) {
      if (NRF24L01_ReadReg(NRF24L01_07_STATUS) & _BV(NRF24L01_07_RX_DR)) {
        XN297_ReadPayload(packet, CX10_packet_length);


        if (packet[0] == 0xAA) {
          //input = String("bind packet");
        } else if (packet[0] == 0x55) {
          String input;
          ppm[AILERON] = (packet[10] << 8) + packet[9];

          input = String("ail: ");
          input += ppm[AILERON];

          ppm[ELEVATOR] = (packet[12] << 8) + packet[11];
          input += " ele: ";
          input += ppm[ELEVATOR];

          ppm[THROTTLE] = (packet[14] << 8) + packet[13];
          input += " thr: ";
          input += ppm[THROTTLE];

          ppm[RUDDER] = (packet[16] << 8) + packet[15];
          input += " rdr: ";
          input += ppm[RUDDER];

          input += " [17]: ";
          char buff[8];
          toBits(buff, packet[17]);
          input += buff;

          input += " [18]: ";
          toBits(buff, packet[18]);
          input += buff;
          Serial.println(input);
        }
    /*
    else {
    input = String("unknown packet:");

    for (int i = 0; i < CX10_packet_length; i++) {
      input += " ";
      input += packet[i];
    }
    input += " len: ";
    input += sizeof(packet);
    input += " ch: ";
    input += CX10_freq[CX10_current_chan];
    Serial.println(input);
    }
    */
      }
    }
  }

  return nextPacket;
}

void CX10RX_init() {
  uint8_t i;
  switch (current_protocol) {
    case PROTO_CX10_BLUE:
      for (i = 0; i < 4; i++) {
        packet[5 + i] = 0xFF;  // clear aircraft ID
      }
      CX10_packet_length = CX10_BLUE_PACKET_LENGTH;
      CX10_packet_period = CX10_BLUE_PACKET_PERIOD;
      break;
    case PROTO_CX10_GREEN:
      CX10_packet_length = CX10_GREEN_PACKET_LENGTH;
      CX10_packet_period = CX10_GREEN_PACKET_PERIOD;
      break;
  }

  CX10_current_chan = 0;
  NRF24L01_Reset();
  NRF24L01_Initialize();
  NRF24L01_SetTxRxMode(RX_EN);
  delay(10);
  XN297_SetTXAddr(CX10_tx_rx_id, 5);
  XN297_SetRXAddr(CX10_tx_rx_id, 5);
  NRF24L01_WriteReg(NRF24L01_05_RF_CH, CX10_RF_BIND_CHANNEL);
  NRF24L01_WriteReg(NRF24L01_07_STATUS, 0x70);                  // Clear data ready, data sent, and retransmit
  NRF24L01_WriteReg(NRF24L01_01_EN_AA, 0x00);                   // No Auto Acknowledgment on all data pipes
  NRF24L01_WriteReg(NRF24L01_11_RX_PW_P0, CX10_packet_length);  // rx pipe 0, [NRF24L01_12_RX_PW_P1]
  NRF24L01_WriteReg(NRF24L01_02_EN_RXADDR, 0x01);               // Enable data pipe 0 only
  NRF24L01_SetBitrate(NRF24L01_BR_1M);                          // 1Mbps
  NRF24L01_SetPower(RF_POWER);
  NRF24L01_WriteReg(NRF24L01_1C_DYNPD, 0x00);    // Disable dynamic payload length on all pipes
  NRF24L01_WriteReg(NRF24L01_1D_FEATURE, 0x00);  // Set feature bits
  delay(150);
}

void CX10RX_bind() {
  uint16_t counter = CX10_GREEN_BIND_COUNT;
  uint32_t timeout;
  while (!bound) {
    switch (current_protocol) {
      case PROTO_CX10_GREEN:
        //TODO:
        break;
      case PROTO_CX10_BLUE:
        delay(1);
        NRF24L01_SetTxRxMode(RX_EN);
        NRF24L01_WriteReg(NRF24L01_07_STATUS, 0x70);
        NRF24L01_FlushRx();
        XN297_Configure(_BV(NRF24L01_00_EN_CRC) | _BV(NRF24L01_00_CRCO) | _BV(NRF24L01_00_PWR_UP) | _BV(NRF24L01_00_PRIM_RX));

        timeout = millis() + 5;
        while (millis() < timeout) {
          if (NRF24L01_ReadReg(NRF24L01_07_STATUS) & _BV(NRF24L01_07_RX_DR)) {
            XN297_ReadPayload(packet, CX10_packet_length);

            String input = String("packet:");
            for (int i = 0; i < CX10_packet_length; i++) {
              input += " ";
              input += packet[i];
            }
            Serial.println(input);

            if (packet[0] == 0xAA) {
              Serial.println("packet[0] == 0xAA");

              NRF24L01_SetTxRxMode(TX_EN);
              XN297_Configure(_BV(NRF24L01_00_EN_CRC) | _BV(NRF24L01_00_CRCO) | _BV(NRF24L01_00_PWR_UP));

              CX10_txid[0] = packet[1];
              CX10_txid[1] = packet[2];
              CX10_txid[2] = packet[3];
              CX10_txid[3] = packet[4];

              CX10_txid[1] &= 0x2F;
              CX10_freq[0] = (CX10_txid[0] & 0x0F) + 0x03;
              CX10_freq[1] = (CX10_txid[0] >> 4) + 0x16;
              CX10_freq[2] = (CX10_txid[1] & 0x0F) + 0x2D;
              CX10_freq[3] = (CX10_txid[1] >> 4) + 0x40;

              packet[5] = CX10_rxid[0];
              packet[6] = CX10_rxid[1];
              packet[7] = CX10_rxid[2];
              packet[8] = CX10_rxid[3];

              //TODO: packet[9] = 0x00;
              packet[9] = 0x01;
              CX10_Write_Packet(0xAA);

              bound = true;
              break;
            }
          }
        }
        break;
    }

    if (ppm[AUX8] > PPM_MAX_COMMAND) {
      reset = true;
      return;
    }
  }
  delay(1);
  NRF24L01_SetTxRxMode(RX_EN);
  NRF24L01_WriteReg(NRF24L01_07_STATUS, 0x70);
  NRF24L01_FlushRx();
  XN297_Configure(_BV(NRF24L01_00_EN_CRC) | _BV(NRF24L01_00_CRCO) | _BV(NRF24L01_00_PWR_UP) | _BV(NRF24L01_00_PRIM_RX));
  //TODO: CX10RX_acknowledgement();
}
void CX10RX_acknowledgement() {
  NRF24L01_SetTxRxMode(TX_EN);
  XN297_Configure(_BV(NRF24L01_00_EN_CRC) | _BV(NRF24L01_00_CRCO) | _BV(NRF24L01_00_PWR_UP));

  Serial.print("TX: ");
  String txidString;
  for (int i = 1; i < 5; i++) {
    txidString += " ";
    txidString += packet[i];
  }
  Serial.println(txidString);

  Serial.print("RX: ");
  String rxidString;
  for (int i = 5; i < 9; i++) {
    rxidString += " ";
    rxidString += packet[i];
  }
  Serial.println(rxidString);

  String channels = String("channels:");
  for (int i = 0; i < 4; i++) {
    channels += " ";
    channels += CX10_freq[i];
  }
  Serial.println(channels);

  packet[9] = 0x01;
  CX10_Write_Packet(0xAA);

  for (int ackcount = 1; ackcount <= 8; ackcount++) {
    NRF24L01_WriteReg(NRF24L01_05_RF_CH, CX10_freq[CX10_current_chan++]);
    CX10_current_chan = CX10_current_chan % CX10_NUM_RF_CHANNELS;

    Serial.print("Ack ");
    Serial.print(ackcount);
    Serial.print(", ch: ");
    Serial.print(CX10_freq[CX10_current_chan]);
    Serial.println(" sent.");
    CX10_Write_Packet(0xAA);

    while (!(NRF24L01_ReadReg(NRF24L01_17_FIFO_STATUS) & _BV(4))) { delay(100); }
    NRF24L01_WriteReg(NRF24L01_05_RF_CH, CX10_freq[CX10_current_chan++]);
    CX10_current_chan = CX10_current_chan % CX10_NUM_RF_CHANNELS;
  }

  NRF24L01_SetTxRxMode(RX_EN);
  NRF24L01_WriteReg(NRF24L01_07_STATUS, 0x70);
  NRF24L01_FlushRx();
  XN297_Configure(_BV(NRF24L01_00_EN_CRC) | _BV(NRF24L01_00_CRCO) | _BV(NRF24L01_00_PWR_UP) | _BV(NRF24L01_00_PRIM_RX));
}