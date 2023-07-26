#include "diveCAN.h"

DiveCAN::DiveCAN(byte in_canID, char *inName) : CAN0{MCP_CAN(10)}
{
  canID = in_canID;
  strncpy(name, inName, CAN_NAME_LENGTH);

  while (CAN0.begin(MCP_ANY) != CAN_OK)
  {
    printf("CAN Begin FAILED\n");
  };

  // Set operation mode to normal so the MCP2515 sends acks to received data.
  if (CAN0.setMode(MCP_NORMAL) != CAN_OK)
  {
    printf("CAN Mode Set FAILED\n");
  }
}

void DiveCAN::HandleInboundMessages()
{
  long unsigned int rxId; // ID of recieved CAN packet
  unsigned char len = 0;  // Length of recieved CAN Packet
  unsigned char rxBuf[8]; // Recieved CAN data
  char msgString[128];    // Array to store serial string

  if (CAN0.checkReceive() == CAN_MSGAVAIL) // If CAN0_INT pin is low, read receive buffer
  {
    CAN0.readMsgBuf(&rxId, &len, rxBuf); // Read data: len = data length, buf = data byte(s)

    if ((rxId & 0x80000000) == 0x80000000) // Determine if ID is standard (11 bits) or extended (29 bits)
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
    else
      sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);

    printf(msgString);

    if ((rxId & 0x40000000) == 0x40000000)
    { // Determine if message is a remote request frame.
      sprintf(msgString, " REMOTE REQUEST FRAME");
      printf(msgString);
    }
    else
    { // If not print the data
      for (byte i = 0; i < len; i++)
      {
        sprintf(msgString, " 0x%.2X", rxBuf[i]);
        printf(msgString);
      }
    }
    printf("\n"); // FFinished printing this packet, newline time

    // While on paper we can just blast stuff out, it makes sense to respond to what the petrel is doing
    if ((rxId & 0x1FFFFFFF) == 0x0D370401 || (rxId & 0x1FFFFFFF) == 0x0D300401)
    {           // Bus Init
      sendID(); // We send the ID every time we send out a message, stops us getting "connection lost"
      sendName();
      sendMillis();
      sendPPO2();
      sendCellsStat();
      sendStatus();
    }

    // The shearwater sends out 0x0D000001 every second, keepalive?
    // We respond by telling it the PPO2, easier than a timer for testing purposes.
    if ((rxId & 0x1FFFFFFF) == 0x0D000001)
    { // Send PPO2
      sendID();
      sendMillis();
      sendPPO2();
      sendCellsStat();
      sendStatus();
    }

    // Respond to a calibrate message, currently only to the happy path
    // Say we're calibrating, then wait a second before finishing.
    // If we send the response immediately after then the shearwater
    // misses it and we time out
    if ((rxId & 0x1FFFFFFF) == 0x0D130201)
    { // Calibrate
      sendCalAck();
      _delay_ms(1000);
      sendCalComplete();
    }

    // We're asked for our menu, send the menu response
    if ((rxId & 0x1FFFFFFF) == 0x0D0A0401 && rxBuf[4] == 0x00)
    { // Menu
      sendMenuAck();
    }

    // We're asked for our menu fields, send our menu field data
    // the 4th byte is our field index so we pass that up
    if ((rxId & 0x1FFFFFFF) == 0x0D0A0401 && (rxBuf[4] & 0x30) == 0x30)
    { // 0x30, we want the field data (I assume type, be that static text or option)
      sendMenuFields(rxBuf[4]);
    }
    else if ((rxId & 0x1FFFFFFF) == 0x0D0A0401 && (rxBuf[4] & 0x10) == 0x10)
    { // 0x10, send the text itself
      sendMenuText(rxBuf[4]);
    }
  }
}

// Raw messages
void DiveCAN::sendID()
{
  // byte data[3] = {0x01, 0x00, 0x09};
  byte data[3] = {0x01, 0x00, 0x00};
  byte sndStat = CAN0.sendMsgBuf(0xD000004, 1, 3, data);
  if (sndStat == CAN_OK)
  {
    printf("ID Sent Successfully!\n");
  }
  else
  {
    printf("Error Sending ID...\n");
  }
}

void DiveCAN::sendName()
{
  byte data1[9] = "CHECKLST";
  byte sndStat = CAN0.sendMsgBuf(0xD010004, 1, 8, data1);
  if (sndStat == CAN_OK)
  {
    printf("Name Sent Successfully!\n");
  }
  else
  {
    printf("Error Sending Name...\n");
  }
}

void DiveCAN::sendMillis()
{
  // byte data[7] = {0x14, 0x18, 0x13, 0x5f, 0x14, 0x20, 0x00};
  byte data[7] = {0x00, 0x00, 0x13, 0x5f, 0x14, 0x20, 0x00};
  byte sndStat = CAN0.sendMsgBuf(0xD110004, 1, 7, data);
  if (sndStat == CAN_OK)
  {
    printf("millis Sent Successfully!\n");
  }
  else
  {
    printf("Error Sending millis...");
  }
}
void DiveCAN::sendPPO2()
{
  // byte data[4] = {0x00, 0xee, 0xee, 0xee};
  byte data[4] = {0x00, 0xFF, 161, 77};
  byte sndStat = CAN0.sendMsgBuf(0xD040004, 1, 4, data);
  if (sndStat == CAN_OK)
  {
    printf("PPO2 Sent Successfully!\n");
  }
  else
  {
    printf("Error Sending PPO2...\n");
  }
}

void DiveCAN::sendCellsStat()
{
  byte data[2] = {0x07, 0x50};
  byte sndStat = CAN0.sendMsgBuf(0xdca0004, 1, 2, data);
  if (sndStat == CAN_OK)
  {
    printf("Cell stat Sent Successfully!\n");
  }
  else
  {
    printf("Error Sending Cell stat...\n");
  }
}

void DiveCAN::sendStatus()
{
  byte data[8] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x15, 0xff, 0x02};
  byte sndStat = CAN0.sendMsgBuf(0xdcb0004, 1, 8, data);
  if (sndStat == CAN_OK)
  {
    printf("Cell stat Sent Successfully!\n");
  }
  else
  {
    printf("Error Sending Cell stat...\n");
  }
}

void DiveCAN::sendCalAck()
{
  byte data[8] = {0x05, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00};
  byte sndStat = CAN0.sendMsgBuf(0xd120004, 1, 8, data);
  if (sndStat == CAN_OK)
  {
    printf("CalACK Sent Successfully!\n");
  }
  else
  {
    printf("Error Sending CalACK...\n");
  }
}

void DiveCAN::sendCalComplete()
{
  byte data[8] = {0x01, 0x34, 0x32, 0x34, 0x64, 0x03, 0xf6, 0x07};
  byte sndStat = CAN0.sendMsgBuf(0xd120004, 1, 8, data);
  if (sndStat == CAN_OK)
  {
    printf("CalComp Sent Successfully!\n");
  }
  else
  {
    printf("Error Sending CalComp...\n");
  }
}

void DiveCAN::sendMenuAck()
{
  byte data[6] = {0x05, 0x00, 0x62, 0x91, 0x00, 0x05};
  byte sndStat = CAN0.sendMsgBuf(0xd0a0104, 1, 6, data);
  if (sndStat == CAN_OK)
  {
    printf("menuAc Sent Successfully!\n");
  }
  else
  {
    printf("Error Sending menuAc...\n");
  }
}

char *listItems[5] = {"O2&DIL ON ", "PRSSURE OK?", "BCD/DRSUIT", "MAV? PPO2? ", "BAILOUT?  "};

void DiveCAN::sendMenuText(byte a)
{
  char *item = listItems[a & 0x0F];
  byte data[8] = {0x10, 0x10, 0x00, 0x62, 0x91, a, item[0], item[1]};
  byte sndStat = CAN0.sendMsgBuf(0xd0a0104, 1, 8, data);
  if (sndStat == CAN_OK)
  {
    printf("menu1 Sent Successfully!\n");
  }
  else
  {
    printf("Error Sending menu1...\n");
  }
  byte data2[8] = {0x21, item[2], item[3], item[4], item[5], item[6], item[7], item[8]};
  sndStat = CAN0.sendMsgBuf(0xd0a0104, 1, 8, data2);
  if (sndStat == CAN_OK)
  {
    printf("menu2 Sent Successfully!\n");
  }
  else
  {
    printf("Error Sending menu2...\n");
  }

  byte data3[4] = {0x22, item[9], 0x00, 0x00};
  sndStat = CAN0.sendMsgBuf(0xd0a0104, 1, 4, data3);
  if (sndStat == CAN_OK)
  {
    printf("menu3 Sent Successfully!\n");
  }
  else
  {
    printf("Error Sending menu3...\n");
  }
}

void DiveCAN::sendMenuFields(byte a)
{
  byte data[8] = {0x10, 0x14, 0x00, 0x62, 0x91, a, 0x00, 0x00};
  byte sndStat = CAN0.sendMsgBuf(0xd0a0104, 1, 8, data);
  if (sndStat == CAN_OK)
  {
    printf("menuf1 Sent Successfully!\n");
  }
  else
  {
    printf("Error Sending menuf1...\n");
  }

  byte data2[8] = {0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00};
  sndStat = CAN0.sendMsgBuf(0xd0a0104, 1, 8, data2);
  if (sndStat == CAN_OK)
  {
    printf("menuf2 Sent Successfully!\n");
  }
  else
  {
    printf("Error Sending menuf2...\n");
  }

  byte data3[8] = {0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  sndStat = CAN0.sendMsgBuf(0xd0a0104, 1, 8, data3);
  if (sndStat == CAN_OK)
  {
    printf("menuf3 Sent Successfully!\n");
  }
  else
  {
    printf("Error Sending menuf3...\n");
  }
}