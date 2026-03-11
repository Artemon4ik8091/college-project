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

void printHash(byte *hash) {
  Serial.print("hash: ");
  for (byte i = 0; i < HASH_SIZE; i++) {
    if (hash[i] < 0x10) Serial.print("0");
    Serial.print(hash[i], HEX);
  }
  Serial.println();
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

bool chargeBalance() {
  delay(250);
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

void onI2CReceive(int count) {
  if (count < 1) return;
  receivedCommand = Wire.read();
  commandPending = true;

  while (Wire.available()) {
    Wire.read();
  }
}

void handleCommand(byte cmd) {
  if (cmd == CMD_APPROVE_AND_CHARGE) {
    Serial.println("approve command received");
    bool charged = chargeBalance();

    if (charged) {
      Serial.println("charge success");
      indicateSuccess();
    } else {
      Serial.println("charge failed");
      indicateFail();
    }

    clearHashBuffer();
  } else if (cmd == CMD_DENY) {
    Serial.println("deny command received");
    indicateFail();
    clearHashBuffer();
  } else {
    Serial.print("unknown command: ");
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

  Serial.println("reader ready");
}

void loop() {
  if (commandPending) {
    noInterrupts();
    byte cmd = receivedCommand;
    commandPending = false;
    interrupts();

    handleCommand(cmd);
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

  Serial.println("card detected");
  computeHashFromFixedString();
  printHash(hashBuffer);

  lastScanTime = millis();

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  delay(100);
}
