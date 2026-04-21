/*
  Электронный распознаватель монет (Ard 1)
  - Поддержка I2C Slave (0x09) для Мастера
  - Динамическая установка цены по I2C
  - Встроенная калибровка (пин 3)
  - Режим энергосбережения
*/

#include <Wire.h>
#include <EEPROMex.h>
#include <LowPower.h>
#include "LCD_1602_RUS.h"

//------- НАСТРОЙКИ ---------
#define I2C_ADDRESS 0x09
#define coin_amount 5    
float coin_value[coin_amount] = {0.5, 1.0, 2.0, 5.0, 10.0}; 
String currency = "RUB";

int coin_signal[coin_amount];
int coin_quantity[coin_amount];  
float summ_money = 0;            
int empty_signal;

volatile float dynamic_target = 0; // Целевая сумма от Мастера
volatile byte goal_status = 0x00;  // Статус для Мастера (0x01 = оплачено)

//------- ПИНЫ ---------
#define button 2         // Пробуждение (INT0)
#define calibr_button 3  // Скрытая кнопка калибровки/сброса
#define disp_power 12    // Питание дисплея
#define LEDpin 8         // Питание светодиода
#define IRpin 17         // Питание фототранзистора (A3)
#define IRsens 14        // Сигнал фототранзистора (A0)

//------- ТАЙМЕРЫ И ФЛАГИ ---------
int stb_time = 10000;
unsigned long standby_timer, reset_timer; 
boolean recogn_flag, sleep_flag = true, coin_flag = false;
int sens_signal, last_sens_signal;

LCD_1602_RUS lcd(0x27, 16, 2);

// === I2C: Мастер запрашивает статус ===
void onI2CRequest() {
  Wire.write(goal_status);
}

// === I2C: Мастер присылает цену ===
void onI2CReceive(int count) {
  if (count >= 1) {
    byte p = Wire.read();
    if (p > 0) {
      dynamic_target = (float)p; 
      checkGoal(); // Проверяем, может уже хватает
    }
  }
  while (Wire.available()) Wire.read(); 
}

// === Проверка достижения цели ===
void checkGoal() {
  if (dynamic_target > 0 && summ_money >= dynamic_target) {
    goal_status = 0x01;
  } else {
    goal_status = 0x00;
  }
}

// === Уход в сон ===
void good_night() {
  // Сохраняем кол-во монет перед сном
  for (byte i = 0; i < coin_amount; i++) {
    EEPROM.updateInt(20 + i * 2, coin_quantity[i]);
  }
  sleep_flag = true;
  digitalWrite(disp_power, 0);
  digitalWrite(LEDpin, 0);
  digitalWrite(IRpin, 0);
  delay(100);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
}

// === Пробуждение ===
void wake_up() {
  digitalWrite(disp_power, 1);
  digitalWrite(LEDpin, 1);
  digitalWrite(IRpin, 1);
  standby_timer = millis();
}

void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(button, INPUT_PULLUP);
  pinMode(calibr_button, INPUT_PULLUP);
  pinMode(disp_power, OUTPUT);
  pinMode(LEDpin, OUTPUT);
  pinMode(IRpin, OUTPUT);

  digitalWrite(disp_power, 1);
  digitalWrite(LEDpin, 1);
  digitalWrite(IRpin, 1);

  attachInterrupt(0, wake_up, CHANGE); // Прерывание на пин 2

  empty_signal = analogRead(IRsens);

  lcd.init();
  lcd.backlight();

  Wire.begin(I2C_ADDRESS);
  Wire.onRequest(onI2CRequest);
  Wire.onReceive(onI2CReceive);

  // === СЕРВИС И КАЛИБРОВКА ===
  if (!digitalRead(calibr_button)) {  
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print(L"Сервис");
    delay(500);
    reset_timer = millis();
    
    // Ждем 3 секунды для сброса памяти
    while (1) {
      if (millis() - reset_timer > 3000) {
        for (byte i = 0; i < coin_amount; i++) {
          coin_quantity[i] = 0;
          EEPROM.writeInt(20 + i * 2, 0);
        }
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(L"Память очищена");
        delay(1000);
      }
      // Если отпустили кнопку - переходим к калибровке
      if (digitalRead(calibr_button)) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(L"Калибровка");
        break;
      }
    }
    
    // Процесс калибровки
    while (1) {
      for (byte i = 0; i < coin_amount; i++) {
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print(L"Киньте монету:");
        lcd.setCursor(0, 1); lcd.print(coin_value[i]);
        lcd.setCursor(13, 1); lcd.print(currency);
        
        last_sens_signal = empty_signal;
        while (1) {
          sens_signal = analogRead(IRsens);
          if (sens_signal > last_sens_signal) last_sens_signal = sens_signal;
          if (sens_signal - empty_signal > 3) coin_flag = true;
          
          if (coin_flag && (abs(sens_signal - empty_signal)) < 2) {
            coin_signal[i] = last_sens_signal;
            EEPROM.writeInt(i * 2, coin_signal[i]);
            coin_flag = false;
            break;
          }
        }
      }
      lcd.clear();
      lcd.print(L"Готово!");
      delay(1000);
      break;
    }
  }

  // === ОБЫЧНЫЙ ЗАПУСК ===
  // Считываем калибровку и считаем баланс
  summ_money = 0;
  for (byte i = 0; i < coin_amount; i++) {
    coin_signal[i] = EEPROM.readInt(i * 2);
    coin_quantity[i] = EEPROM.readInt(20 + i * 2);
    summ_money += coin_quantity[i] * coin_value[i];
  }

  checkGoal();
  standby_timer = millis();
}

void loop() {
  if (sleep_flag) {
    delay(500);
    lcd.init();
    lcd.clear();

    if (goal_status == 0x01) {
      lcd.setCursor(0, 0); lcd.print(L"ОПЛАЧЕНО!");
    } else if (dynamic_target > 0) {
      lcd.setCursor(0, 0); lcd.print(L"Цена: "); lcd.print((int)dynamic_target);
    } else {
      lcd.setCursor(0, 0); lcd.print(L"Сделайте выбор");
    }
    
    lcd.setCursor(0, 1); lcd.print(summ_money);
    lcd.setCursor(13, 1); lcd.print(currency);
    
    empty_signal = analogRead(IRsens);
    sleep_flag = false;
  }

  last_sens_signal = empty_signal;
  
  while (1) {
    sens_signal = analogRead(IRsens);
    if (sens_signal > last_sens_signal) last_sens_signal = sens_signal;
    if (sens_signal - empty_signal > 3) coin_flag = true;
    
    if (coin_flag && (abs(sens_signal - empty_signal)) < 2) {
      recogn_flag = false;
      for (byte i = 0; i < coin_amount; i++) {
        int delta = abs(last_sens_signal - coin_signal[i]);
        if (delta < 30) {
          summ_money += coin_value[i];
          coin_quantity[i]++;
          recogn_flag = true;

          // Обновляем экран
          lcd.setCursor(0, 1); 
          lcd.print(summ_money);
          lcd.print(L"    "); // Затираем старые символы

          checkGoal(); 
          if (goal_status == 0x01) {
            lcd.setCursor(0, 0);
            lcd.print(L"ОПЛАЧЕНО!       ");
          }
          break;
        }
      }
      coin_flag = false;
      standby_timer = millis();
      break;
    }

    if (millis() - standby_timer > stb_time) {
      good_night();
      break;
    }

    // Просмотр статистики при удержании кнопки (пин 2)
    while (!digitalRead(button)) {
      if (millis() - standby_timer > 2000) {
        lcd.clear();
        for (byte i = 0; i < coin_amount; i++) {
          lcd.setCursor(i * 3, 0);
          lcd.print((int)coin_value[i]);
          lcd.setCursor(i * 3, 1); lcd.print(coin_quantity[i]);
        }
      }
    }
  }
}