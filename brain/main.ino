#include <Wire.h>

#define READER_ADDRESS 0x08
#define PIGGYBANK_ADDRESS 0x09
#define HASH_SIZE 32

#define CMD_DENY 0xA0
#define CMD_APPROVE_AND_CHARGE 0xA1

// Кнопки [cite: 1]
const uint8_t BTN_DRINK1  = 2;
const uint8_t BTN_DRINK2  = 3;
const uint8_t BTN_VOLUME1 = 4; 
const uint8_t BTN_VOLUME2 = 5;

// Помпы [cite: 4]
const uint8_t PUMP1_PIN = 9;
const uint8_t PUMP2_PIN = 10;

// ТАЙМИНГИ (в миллисекундах)
const unsigned long PUMP1_FULL_TIME = 2000; // 2 сек [cite: 31]
const unsigned long PUMP2_FULL_TIME = 4000; // 4 сек [cite: 32]

// ЦЕННИКИ
const uint8_t PRICE_SMALL = 50;
const uint8_t PRICE_LARGE = 100;

bool goalReached = false;
bool orderDone = false;
uint8_t selectedDrink = 0;
uint8_t selectedVolume = 0;

bool lastDrink1State  = HIGH;
bool lastDrink2State  = HIGH;
bool lastVolume1State = HIGH;
bool lastVolume2State = HIGH;

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
const unsigned long piggyPollInterval = 5000;

// --- Служебные функции ---
bool sameArray(byte *a, byte *b, byte len) {
  for (byte i = 0; i < len; i++) if (a[i] != b[i]) return false;
  return true;
}

void copyArray(byte *src, byte *dst, byte len) {
  for (byte i = 0; i < len; i++) dst[i] = src[i];
}

bool isAllZero(byte *data, byte len) {
  for (byte i = 0; i < len; i++) if (data[i] != 0) return false;
  return true;
}

bool hashMatches() {
  for (byte i = 0; i < HASH_SIZE; i++) if (packet[i] != authorizedHash[i]) return false;
  return true;
}

// Отправка команды и цены на ридер [cite: 17, 49]
void sendCommand(byte cmd, byte price) {
  Wire.beginTransmission(READER_ADDRESS);
  Wire.write(cmd);
  if (cmd == CMD_APPROVE_AND_CHARGE) {
    Wire.write(price); 
  }
  Wire.endTransmission();
}

// --- Логика Копилки ---
void wakePiggybank() {
  pinMode(PIGGYBANK_WAKE_PIN, OUTPUT);
  digitalWrite(PIGGYBANK_WAKE_PIN, LOW);
  delay(10);
  digitalWrite(PIGGYBANK_WAKE_PIN, HIGH);
  pinMode(PIGGYBANK_WAKE_PIN, INPUT);
  delay(600);
}

void pollPiggybank() {
  if (selectedDrink == 0 || selectedVolume == 0) return; // Не будим копилку без выбора

  wakePiggybank();
  uint8_t count = Wire.requestFrom((uint8_t)PIGGYBANK_ADDRESS, (uint8_t)1);
  if (count < 1) return;

  uint8_t status = Wire.read();
  if (status == 0x01 && !goalReached) {
    goalReached = true;
    orderDone = false;
    Serial.println("Payment: OK (Coins)");
  }
}

// --- УПРАВЛЕНИЕ НАЛИВОМ ---
void tryDispense() {
  if (!goalReached || orderDone) return;
  if (selectedDrink == 0 || selectedVolume == 0) return;

  // Выбираем базовое время для конкретной помпы
  unsigned long baseTime = (selectedDrink == 1) ? PUMP1_FULL_TIME : PUMP2_FULL_TIME;
  
  // Если объем 1 (малый), берем половину времени
  unsigned long finalTime = (selectedVolume == 1) ? (baseTime / 2) : baseTime;
  
  uint8_t activePump = (selectedDrink == 1) ? PUMP1_PIN : PUMP2_PIN;

  Serial.print("Dispensing... Drink: ");
  Serial.print(selectedDrink);
  Serial.print(" Time: ");
  Serial.println(finalTime);

  digitalWrite(activePump, HIGH);
  delay(finalTime);
  digitalWrite(activePump, LOW);

  // Сброс состояния для нового заказа
  orderDone = true;
  goalReached = false;
  selectedDrink = 0;
  selectedVolume = 0;
}

void setup() {
  Serial.begin(9600);
  pinMode(BTN_DRINK1,  INPUT_PULLUP);
  pinMode(BTN_DRINK2,  INPUT_PULLUP);
  pinMode(BTN_VOLUME1, INPUT_PULLUP);
  pinMode(BTN_VOLUME2, INPUT_PULLUP);
  pinMode(PUMP1_PIN,   OUTPUT);
  pinMode(PUMP2_PIN,   OUTPUT);
  pinMode(PIGGYBANK_WAKE_PIN, INPUT);

  digitalWrite(PUMP1_PIN, LOW);
  digitalWrite(PUMP2_PIN, LOW);
  
  Wire.begin();
  Serial.println("System Ready. Select drink and volume.");
}

void handleButtons() {
  bool d1 = digitalRead(BTN_DRINK1);
  bool d2 = digitalRead(BTN_DRINK2);
  bool v1 = digitalRead(BTN_VOLUME1);
  bool v2 = digitalRead(BTN_VOLUME2);

  if (lastDrink1State == HIGH && d1 == LOW) { selectedDrink = 1; Serial.println("Selected: Drink 1"); delay(50); }
  if (lastDrink2State == HIGH && d2 == LOW) { selectedDrink = 2; Serial.println("Selected: Drink 2"); delay(50); }
  if (lastVolume1State == HIGH && v1 == LOW) { selectedVolume = 1; Serial.println("Selected: Small Volume"); delay(50); }
  if (lastVolume2State == HIGH && v2 == LOW) { selectedVolume = 2; Serial.println("Selected: Large Volume"); delay(50); }

  lastDrink1State = d1; lastDrink2State = d2;
  lastVolume1State = v1; lastVolume2State = v2;
}

void loop() {
  handleButtons();

  // Опрос RFID
  int count = Wire.requestFrom(READER_ADDRESS, HASH_SIZE);
  if (count == HASH_SIZE) {
    for (byte i = 0; i < HASH_SIZE; i++) packet[i] = Wire.read();

    if (!isAllZero(packet, HASH_SIZE) && !sameArray(packet, lastPacket, HASH_SIZE)) {
      copyArray(packet, lastPacket, HASH_SIZE);
      
      // Платить можно только после выбора [cite: 27]
      if (selectedDrink != 0 && selectedVolume != 0) {
        if (hashMatches()) {
          uint8_t price = (selectedVolume == 1) ? PRICE_SMALL : PRICE_LARGE;
          Serial.print("Card OK. Charging: ");
          Serial.println(price);
          
          sendCommand(CMD_APPROVE_AND_CHARGE, price);
          goalReached = true; // Теперь помпа включится 
          orderDone = false;
        } else {
          Serial.println("Invalid Card");
          sendCommand(CMD_DENY, 0);
        }
      } else {
        Serial.println("Please select drink first!");
        sendCommand(CMD_DENY, 0);
      }
    }
  }

  // Опрос копилки [cite: 51]
  if (millis() - lastPiggyPoll >= piggyPollInterval) {
    lastPiggyPoll = millis();
    pollPiggybank();
  }

  tryDispense();
  delay(100);
}
