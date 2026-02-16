#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN 9
#define SS_PIN 10
#define PIN_YES 3  // если сошлось
#define PIN_NO 2   // если не сошлось

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;
byte ndefKey[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};

// заготовленный текст для проверки
String secret = "Привет!"; 

void setup() {
    Serial.begin(9600);
    SPI.begin();
    rfid.PCD_Init();
    for (byte i = 0; i < 6; i++) key.keyByte[i] = ndefKey[i];

    pinMode(PIN_YES, OUTPUT);
    pinMode(PIN_NO, OUTPUT);
    digitalWrite(PIN_YES, LOW);
    digitalWrite(PIN_NO, LOW);
    
    Serial.println("приложи метку...");
}

void loop() {
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

    String readString = "";
    byte buffer[18];
    byte size = sizeof(buffer);
    
    if (rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 4, &key, &(rfid.uid)) != MFRC522::STATUS_OK) return;
    if (rfid.MIFARE_Read(4, buffer, &size) != MFRC522::STATUS_OK) return;

    int totalLen = buffer[1]; 
    int readData = 0;
    int currentBlock = 4;
    int skipHeader = 9;

    while (readData < totalLen + 2) {
        if ((currentBlock + 1) % 4 == 0) {
            currentBlock++;
            continue;
        }
        if (currentBlock % 4 == 0) {
            rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, currentBlock, &key, &(rfid.uid));
        }

        size = sizeof(buffer);
        if (rfid.MIFARE_Read(currentBlock, buffer, &size) == MFRC522::STATUS_OK) {
            for (byte i = 0; i < 16 && readData < totalLen + 2; i++) {
                if (readData == 6) skipHeader = 7 + (buffer[i] & 0x3F);
                if (readData >= skipHeader) {
                    readString += (char)buffer[i]; // копим текст в строку
                }
                readData++;
            }
        }
        currentBlock++;
        if (currentBlock > 62) break;
    }

    Serial.print("прочитано: "); Serial.println(readString);

    // проверяем че там прилетело
    if (readString == secret) {
        Serial.println("доступ разрешен {joy}");
        digitalWrite(PIN_YES, HIGH);
        digitalWrite(PIN_NO, LOW);
    } else {
        Serial.println("вали отсюда {warning}");
        digitalWrite(PIN_YES, LOW);
        digitalWrite(PIN_NO, HIGH);
    }

    delay(2000); // даем время светодиоду или реле сработать
    digitalWrite(PIN_YES, LOW);
    digitalWrite(PIN_NO, LOW);

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
}
