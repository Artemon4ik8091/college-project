#include <Wire.h>

#define READER_ADDRESS 0x08
#define PIGGYBANK_ADDRESS 0x09
#define HASH_SIZE 32

#define CMD_DENY 0xA0
#define CMD_APPROVE_AND_CHARGE 0xA1

// Пин Ard3 который подключён к пину 2 (INT0) на Ard1 для пробуждения
#define PIGGYBANK_WAKE_PIN 6

byte packet[HASH_SIZE];
byte lastPacket[HASH_SIZE];

const byte authorizedHash[HASH_SIZE] = {
  0xC4, 0xFF, 0x71, 0x69, 0xF4, 0x32, 0xF7, 0x8E,
  0x7D, 0x72, 0x4B, 0x1C, 0x8E, 0x5C, 0x32, 0x1B,
  0x7C, 0xB1, 0x8D, 0x80, 0xBC, 0x7F, 0x5B, 0xF6,
  0x18, 0x46, 0xBE, 0x2F, 0x14, 0x6E, 0xDF, 0x34
};

bool goalReached = false;

unsigned long lastPiggyPoll = 0;
const unsigned long piggyPollInterval = 5000;  // поллим копилку каждые 5 сек

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

// === Разбудить копилку импульсом на пин 2 ===
void wakePiggybank() {
  pinMode(PIGGYBANK_WAKE_PIN, OUTPUT);
  digitalWrite(PIGGYBANK_WAKE_PIN, LOW);   // дёргаем LOW (пин 2 на Ard1 подтянут к HIGH через INPUT_PULLUP)
  delay(10);                                // 10 мс достаточно для срабатывания INT0 CHANGE
  digitalWrite(PIGGYBANK_WAKE_PIN, HIGH);
  pinMode(PIGGYBANK_WAKE_PIN, INPUT);       // отпускаем пин (Hi-Z), чтобы не мешать кнопке/монете
  delay(600);                               // даём Ard1 время проснуться и поднять I2C
}

// === Запросить статус копилки ===
void pollPiggybank() {
  wakePiggybank();

  int count = Wire.requestFrom(PIGGYBANK_ADDRESS, (byte)1);
  if (count < 1) {
    Serial.println("piggybank: no response");
    return;
  }

  byte status = Wire.read();
  while (Wire.available()) Wire.read();  // сбросить лишнее

  if (status == 0x01 && !goalReached) {
    goalReached = true;
    Serial.println("piggybank: TARGET SUM REACHED!");
    // тут можешь добавить свою логику — зажечь LED, отправить команду и т.д.
  } else if (status == 0x00) {
    goalReached = false;  // если сумму сбросили
  }
}

void setup() {
  Serial.begin(9600);

  Wire.begin();
  Wire.setClock(100000);

  // wake pin по умолчанию Hi-Z чтобы не мешать
  pinMode(PIGGYBANK_WAKE_PIN, INPUT);

  for (byte i = 0; i < HASH_SIZE; i++) {
    packet[i] = 0;
    lastPacket[i] = 0;
  }

  Serial.println("receiver ready");
}

void loop() {
  // === Поллинг RFID ридера (Ard 2) ===
  int count = Wire.requestFrom(READER_ADDRESS, HASH_SIZE);

  if (count != HASH_SIZE) {
    Serial.print("i2c read error, got bytes: ");
    Serial.println(count);

    while (Wire.available()) {
      Wire.read();
    }

    delay(300);
  } else {
    for (byte i = 0; i < HASH_SIZE; i++) {
      packet[i] = Wire.read();
    }

    if (isAllZero(packet, HASH_SIZE)) {
      clearLastPacket();
    } else if (!sameArray(packet, lastPacket, HASH_SIZE)) {
      copyArray(packet, lastPacket, HASH_SIZE);
      printHash(packet);

      if (hashMatches()) {
        Serial.println("hash ok, send charge command");
        sendCommand(CMD_APPROVE_AND_CHARGE);
      } else {
        Serial.println("hash invalid, deny");
        sendCommand(CMD_DENY);
      }
    }
  }

  // === Поллинг копилки (Ard 1) каждые N секунд ===
  if (millis() - lastPiggyPoll >= piggyPollInterval) {
    lastPiggyPoll = millis();
    pollPiggybank();
  }

  delay(200);
}
