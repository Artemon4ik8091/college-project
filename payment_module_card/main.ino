#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SHA256.h>

#define I2C_ADDRESS 0x08
#define HASH_SIZE 32

#define SS_PIN 10
#define RST_PIN 9

#define LED_OK 4
#define LED_FAIL 5

#define CMD_DENY 0xA0
#define CMD_APPROVE_AND_CHARGE 0xA1

MFRC522 mfrc522(SS_PIN, RST_PIN);
SHA256 sha256;

byte hashBuffer[HASH_SIZE];
bool hashReady = false;

volatile bool commandPending = false;
volatile byte receivedCommand = 0;
volatile byte receivedPrice = 0; // Переменная для хранения полученной цены [cite: 100]

unsigned long lastScanTime = 0;
const unsigned long scanCooldown = 1500;

const char fixedString[] = "aswer";

void clearHashBuffer() {
  for (byte i = 0; i < HASH_SIZE; i++) {
    hashBuffer[i] = 0;
  }
  hashReady = false;
}

void computeHashFromFixedString() {
  sha256.reset();
  sha256.update((const uint8_t*)fixedString, strlen(fixedString));
  sha256.finalize(hashBuffer, HASH_SIZE);
  hashReady = true;
}

void indicateSuccess() {
  digitalWrite(LED_OK, HIGH);
  delay(500);
  digitalWrite(LED_OK, LOW);
}

void indicateFail() {
  for (byte i = 0; i < 3; i++) {
    digitalWrite(LED_FAIL, HIGH);
    delay(180);
    digitalWrite(LED_FAIL, LOW);
    delay(180);
  }
}

// Теперь функция принимает цену для списания [cite: 107]
bool chargeBalance(byte price) {
  Serial.print("Charging amount: ");
  Serial.println(price);
  delay(250); // Имитация процесса оплаты
  return true;
}

void onI2CRequest() {
  if (hashReady) {
    Wire.write(hashBuffer, HASH_SIZE);
  } else {
    byte empty[HASH_SIZE];
    for (byte i = 0; i < HASH_SIZE; i++) empty[i] = 0;
    Wire.write(empty, HASH_SIZE);
  }
}

// Обновленная функция приема данных по I2C 
void onI2CReceive(int count) {
  if (count < 1) return;
  
  receivedCommand = Wire.read(); // Читаем первый байт (команду) 
  
  if (count >= 2) {
    receivedPrice = Wire.read(); // Читаем второй байт (цену), если он есть 
  } else {
    receivedPrice = 0;
  }
  
  commandPending = true;
  
  while (Wire.available()) {
    Wire.read(); // Очищаем буфер, если пришло что-то лишнее [cite: 111]
  }
}

void handleCommand(byte cmd, byte price) {
  if (cmd == CMD_APPROVE_AND_CHARGE) {
    Serial.print("Approve command received. Price: ");
    Serial.println(price);
    
    bool charged = chargeBalance(price); // Передаем цену в логику оплаты [cite: 112]

    if (charged) {
      Serial.println("Charge success");
      indicateSuccess();
    } else {
      Serial.println("Charge failed");
      indicateFail();
    }

    clearHashBuffer();
  } else if (cmd == CMD_DENY) {
    Serial.println("Deny command received");
    indicateFail();
    clearHashBuffer();
  } else {
    Serial.print("Unknown command: ");
    Serial.println(cmd, HEX);
    indicateFail();
    clearHashBuffer();
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(LED_OK, OUTPUT);
  pinMode(LED_FAIL, OUTPUT);
  digitalWrite(LED_OK, LOW);
  digitalWrite(LED_FAIL, LOW);

  SPI.begin();
  mfrc522.PCD_Init();

  Wire.begin(I2C_ADDRESS);
  Wire.onRequest(onI2CRequest);
  Wire.onReceive(onI2CReceive);

  clearHashBuffer();
  Serial.println("Reader ready");
}

void loop() {
  if (commandPending) {
    noInterrupts();
    byte cmd = receivedCommand;
    byte price = receivedPrice; // Забираем цену из волатильной переменной [cite: 117]
    commandPending = false;
    interrupts();

    handleCommand(cmd, price);
  }

  if (millis() - lastScanTime < scanCooldown) {
    delay(30);
    return;
  }

  if (!mfrc522.PICC_IsNewCardPresent()) {
    delay(30);
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    delay(30);
    return;
  }

  Serial.println("Card detected");
  computeHashFromFixedString();
  
  lastScanTime = millis();
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  delay(100);
}
