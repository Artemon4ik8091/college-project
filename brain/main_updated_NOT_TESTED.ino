#include <Wire.h>

#define READER_ADDRESS 0x08
#define PIGGYBANK_ADDRESS 0x09
#define HASH_SIZE 32

#define CMD_DENY 0xA0
#define CMD_APPROVE_AND_CHARGE 0xA1

// кнопки
const uint8_t BTN_DRINK1  = 2;   // напиток 1
const uint8_t BTN_DRINK2  = 3;   // напиток 2
const uint8_t BTN_VOLUME1 = 4;   // объем 1
const uint8_t BTN_VOLUME2 = 5;   // объем 2

// помпы
const uint8_t PUMP1_PIN = 9;     // помпа напитка 1
const uint8_t PUMP2_PIN = 10;    // помпа напитка 2

// время налива для 2 объемов
const unsigned long VOLUME1_TIME = 3000;  // малый объем
const unsigned long VOLUME2_TIME = 5000;  // большой объем

bool goalReached = false;
bool orderDone = false;

uint8_t selectedDrink = 0;
uint8_t selectedVolume = 0;

bool lastDrink1State  = HIGH;
bool lastDrink2State  = HIGH;
bool lastVolume1State = HIGH;
bool lastVolume2State = HIGH;

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

  uint8_t count = Wire.requestFrom((uint8_t)PIGGYBANK_ADDRESS, (uint8_t)1);
  if (count < 1) {
    Serial.println("piggybank: no response");
    return;
  }

  uint8_t status = Wire.read();
  while (Wire.available()) Wire.read();

  if (status == 0x01 && !goalReached) {
    goalReached = true;
    orderDone = false;
    Serial.println("piggybank: target sum reached!");
  } else if (status == 0x00) {
    goalReached = false;
    orderDone = false;
  }
}

void tryDispense() {
  if (!goalReached) return;
  if (orderDone) return;
  if (selectedDrink == 0 || selectedVolume == 0) return;

  unsigned long pumpTime = 0;

  if (selectedVolume == 1) {
    pumpTime = VOLUME1_TIME;
  } else if (selectedVolume == 2) {
    pumpTime = VOLUME2_TIME;
  }

  if (selectedDrink == 1) {
    Serial.println("dispensing drink 1");
    digitalWrite(PUMP1_PIN, HIGH);
    delay(pumpTime);
    digitalWrite(PUMP1_PIN, LOW);
  } else if (selectedDrink == 2) {
    Serial.println("dispensing drink 2");
    digitalWrite(PUMP2_PIN, HIGH);
    delay(pumpTime);
    digitalWrite(PUMP2_PIN, LOW);
  }

  orderDone = true;
  selectedDrink = 0;
  selectedVolume = 0;
}

void setup() {
  Serial.begin(9600);
  pinMode(9, OUTPUT);

  pinMode(BTN_DRINK1, INPUT_PULLUP);
  pinMode(BTN_DRINK2, INPUT_PULLUP);
  pinMode(BTN_VOLUME1, INPUT_PULLUP);
  pinMode(BTN_VOLUME2, INPUT_PULLUP);

  pinMode(PUMP1_PIN, OUTPUT);
  pinMode(PUMP2_PIN, OUTPUT);

  digitalWrite(PUMP1_PIN, LOW);
  digitalWrite(PUMP2_PIN, LOW);

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

void handleButtons() {
  bool d1 = digitalRead(BTN_DRINK1);
  bool d2 = digitalRead(BTN_DRINK2);
  bool v1 = digitalRead(BTN_VOLUME1);
  bool v2 = digitalRead(BTN_VOLUME2);

  if (lastDrink1State == HIGH && d1 == LOW) {
    selectedDrink = 1;
    Serial.println("selected drink 1");
    delay(30);
  }

  if (lastDrink2State == HIGH && d2 == LOW) {
    selectedDrink = 2;
    Serial.println("selected drink 2");
    delay(30);
  }

  if (lastVolume1State == HIGH && v1 == LOW) {
    selectedVolume = 1;
    Serial.println("selected volume 1");
    delay(30);
  }

  if (lastVolume2State == HIGH && v2 == LOW) {
    selectedVolume = 2;
    Serial.println("selected volume 2");
    delay(30);
  }

  lastDrink1State = d1;
  lastDrink2State = d2;
  lastVolume1State = v1;
  lastVolume2State = v2;
}

void loop() {
  // === Поллинг RFID ридера (Ard 2) ===
  int count = Wire.requestFrom(READER_ADDRESS, HASH_SIZE);

  handleButtons();
  tryDispense();

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
