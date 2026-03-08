#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN 9
#define SS_PIN 10
#define PIN_YES 3  
#define PIN_NO 2   

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;
byte ndefKey[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};

int price = 139; // наш ценник

void setup() {
    Serial.begin(9600);
    SPI.begin();
    rfid.PCD_Init();
    for (byte i = 0; i < 6; i++) key.keyByte[i] = ndefKey[i];
    pinMode(PIN_YES, OUTPUT);
    pinMode(PIN_NO, OUTPUT);
    Serial.println("готов к работе с телефонными метками...");
}

void loop() {
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

    byte blockAddr = 4; 
    byte buffer[18];
    byte size = sizeof(buffer);

    // авторизация (используем ключ для ndef)
    if (rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockAddr, &key, &(rfid.uid)) != MFRC522::STATUS_OK) {
        Serial.println("ошибка ключа {warning}");
        return;
    }

    // читаем блок целиком (со всеми заголовками смартфона)
    if (rfid.MIFARE_Read(blockAddr, buffer, &size) != MFRC522::STATUS_OK) return;

    String rawDigits = "";
    int firstDigitIndex = -1;

    // ищем, где в блоке начинаются цифры и запоминаем позицию
    for (int i = 0; i < 16; i++) {
        if (isDigit(buffer[i])) {
            if (firstDigitIndex == -1) firstDigitIndex = i;
            rawDigits += (char)buffer[i];
        }
    }

    if (firstDigitIndex != -1 && rawDigits.length() > 0) {
        int currentBalance = rawDigits.toInt();
        Serial.print("баланс с телефона: "); Serial.println(currentBalance);

        if (currentBalance >= price) {
            int newBalance = currentBalance - price;
            String newStr = String(newBalance);

            // критический момент: вписываем новое число ПОВЕРХ старого в ТОТ ЖЕ буфер
            // мы не трогаем buffer[0], buffer[1] и т.д., где лежат метки NDEF
            for (int i = 0; i < newStr.length(); i++) {
                buffer[firstDigitIndex + i] = newStr[i];
            }
            
            // если новое число короче старого (н-р было 100 стало 50), забиваем лишнюю цифру пробелом
            if (newStr.length() < rawDigits.length()) {
                for (int i = newStr.length(); i < rawDigits.length(); i++) {
                    buffer[firstDigitIndex + i] = ' '; 
                }
            }

            // пишем измененный буфер обратно
            if (rfid.MIFARE_Write(blockAddr, buffer, 16) == MFRC522::STATUS_OK) {
                Serial.print("успех! новый баланс: "); Serial.println(newBalance);
                digitalWrite(PIN_YES, HIGH);
                delay(1000);
                digitalWrite(PIN_YES, LOW);
            }
        } else {
            Serial.println("недостаточно средств");
            digitalWrite(PIN_NO, HIGH);
            delay(1000);
            digitalWrite(PIN_NO, LOW);
        }
    } else {
        Serial.println("цифры не найдены, запиши число через NFC Tools");
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
}
