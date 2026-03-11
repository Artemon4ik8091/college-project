#include <Wire.h>

#define READER_ADDRESS 0x08
#define HASH_SIZE 32

#define CMD_DENY 0xA0
#define CMD_APPROVE_AND_CHARGE 0xA1

byte packet[HASH_SIZE];
byte lastPacket[HASH_SIZE];

const byte authorizedHash[HASH_SIZE] = {
  0xC4, 0xFF, 0x71, 0x69, 0xF4, 0x32, 0xF7, 0x8E,
  0x7D, 0x72, 0x4B, 0x1C, 0x8E, 0x5C, 0x32, 0x1B,
  0x7C, 0xB1, 0x8D, 0x80, 0xBC, 0x7F, 0x5B, 0xF6,
  0x18, 0x46, 0xBE, 0x2F, 0x14, 0x6E, 0xDF, 0x34
};

bool sameArray(byte *a, byte *b, byte len) {
  for (byte i = 0; i < len; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

void copyArray(byte *src, byte *dst, byte len) {
  for (byte i = 0; i < len; i++) {
    dst[i] = src[i];
  }
}

bool isAllZero(byte *data, byte len) {
  for (byte i = 0; i < len; i++) {
    if (data[i] != 0) return false;
  }
  return true;
}

bool hashMatches() {
  for (byte i = 0; i < HASH_SIZE; i++) {
    if (packet[i] != authorizedHash[i]) return false;
  }
  return true;
}

void printHash(byte *hash) {
  Serial.print("hash: ");
  for (byte i = 0; i < HASH_SIZE; i++) {
    if (hash[i] < 0x10) Serial.print("0");
    Serial.print(hash[i], HEX);
  }
  Serial.println();
}

void sendCommand(byte cmd) {
  Wire.beginTransmission(READER_ADDRESS);
  Wire.write(cmd);
  Wire.endTransmission();
}

void clearLastPacket() {
  for (byte i = 0; i < HASH_SIZE; i++) {
    lastPacket[i] = 0;
  }
}

void setup() {
  Serial.begin(9600);

  Wire.begin();
  Wire.setClock(100000);

  for (byte i = 0; i < HASH_SIZE; i++) {
    packet[i] = 0;
    lastPacket[i] = 0;
  }

  Serial.println("receiver ready");
}

void loop() {
  int count = Wire.requestFrom(READER_ADDRESS, HASH_SIZE);

  if (count != HASH_SIZE) {
    Serial.print("i2c read error, got bytes: ");
    Serial.println(count);

    while (Wire.available()) {
      Wire.read();
    }

    delay(300);
    return;
  }

  for (byte i = 0; i < HASH_SIZE; i++) {
    packet[i] = Wire.read();
  }

  if (isAllZero(packet, HASH_SIZE)) {
    clearLastPacket();
    delay(200);
    return;
  }

  if (sameArray(packet, lastPacket, HASH_SIZE)) {
    delay(200);
    return;
  }

  copyArray(packet, lastPacket, HASH_SIZE);
  printHash(packet);

  if (hashMatches()) {
    Serial.println("hash ok, send charge command");
    sendCommand(CMD_APPROVE_AND_CHARGE);
  } else {
    Serial.println("hash invalid, deny");
    sendCommand(CMD_DENY);
  }

  delay(500);
}
