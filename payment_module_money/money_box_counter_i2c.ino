/*
  Электронный распознаватель монет (по размеру) для копилки со счётчиком суммы и статистикой по каждому типу монет.
  + I2C slave (адрес 0x09) — отдаёт статус цели мастеру (Ard 3)
  + Пробуждение по пину 2 (INT0) — мастер кратковременно дёргает этот пин
*/

//-------НАСТРОЙКИ---------
#define coin_amount 5    // число монет, которые нужно распознать
float coin_value[coin_amount] = {0.5, 1.0, 2.0, 5.0, 10.0};  // стоимость монет
String currency = "RUB"; // валюта (английские буквы!!!)
int stb_time = 10000;    // время бездействия, через которое система уйдёт в сон (миллисекунды)

#define TARGET_SUM 137.0   // целевая сумма (меняй под себя)
#define I2C_ADDRESS 0x09   // адрес этой ардуино на шине I2C
//-------НАСТРОЙКИ---------

int coin_signal[coin_amount];    // тут хранится значение сигнала для каждого размера монет
int coin_quantity[coin_amount];  // количество монет
byte empty_signal;               // храним уровень пустого сигнала
unsigned long standby_timer, reset_timer; // таймеры
float summ_money = 0;            // сумма монет в копилке

//-------БИБЛИОТЕКИ---------
#include "LowPower.h"
#include "EEPROMex.h"
#include "LCD_1602_RUS.h"
#include <Wire.h>
//-------БИБЛИОТЕКИ---------

LCD_1602_RUS lcd(0x27, 16, 2);            // создать дисплей
// если дисплей не работает, замени 0x27 на 0x3f

boolean recogn_flag, sleep_flag = true;   // флажки

// I2C статус: 0x00 = цель не достигнута, 0x01 = достигнута
volatile byte goal_status = 0x00;

//-------ПИНЫ---------
#define button 2         // кнопка "проснуться" (INT0) — сюда же Ard3 подаёт импульс
#define calibr_button 3  // скрытая кнопка калибровки и сброса
#define disp_power 12    // питание дисплея
#define LEDpin 8         // питание светодиода
#define IRpin 17         // питание фототранзистора
#define IRsens 14        // сигнал фототранзистора
//-------ПИНЫ---------
int sens_signal, last_sens_signal;
boolean coin_flag = false;

// === I2C: мастер запрашивает статус ===
void onI2CRequest() {
  Wire.write(goal_status);
}

// === Проверка цели ===
void checkGoal() {
  if (summ_money >= TARGET_SUM) {
    goal_status = 0x01;
  } else {
    goal_status = 0x00;
  }
}

void setup() {
  Serial.begin(9600);
  delay(500);

  // подтягиваем кнопки
  pinMode(button, INPUT_PULLUP);
  pinMode(calibr_button, INPUT_PULLUP);

  // пины питания как выходы
  pinMode(disp_power, OUTPUT);
  pinMode(LEDpin, OUTPUT);
  pinMode(IRpin, OUTPUT);

  // подать питание на дисплей и датчик
  digitalWrite(disp_power, 1);
  digitalWrite(LEDpin, 1);
  digitalWrite(IRpin, 1);

  // подключить прерывание для пробуждения
  attachInterrupt(0, wake_up, CHANGE);

  empty_signal = analogRead(IRsens);  // считать пустой (опорный) сигнал

  // инициализация дисплея
  lcd.init();
  lcd.backlight();

  // === I2C slave ===
  Wire.begin(I2C_ADDRESS);
  Wire.onRequest(onI2CRequest);

  if (!digitalRead(calibr_button)) {  // если при запуске нажата кнопка калибровки
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print(L"Сервис");
    delay(500);
    reset_timer = millis();
    while (1) {
      if (millis() - reset_timer > 3000) {
        for (byte i = 0; i < coin_amount; i++) {
          coin_quantity[i] = 0;
          EEPROM.writeInt(20 + i * 2, 0);
        }
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(L"Память очищена");
        delay(100);
      }
      if (digitalRead(calibr_button)) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(L"Калибровка");
        break;
      }
    }
    while (1) {
      for (byte i = 0; i < coin_amount; i++) {
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
      break;
    }
  }

  // считать из памяти сигналы монет и их количество
  for (byte i = 0; i < coin_amount; i++) {
    coin_signal[i] = EEPROM.readInt(i * 2);
    coin_quantity[i] = EEPROM.readInt(20 + i * 2);
    summ_money += coin_quantity[i] * coin_value[i];
  }

  checkGoal();  // проверить цель при старте

  standby_timer = millis();
}

void loop() {
  if (sleep_flag) {
    delay(500);
    lcd.init();
    lcd.clear();

    if (goal_status == 0x01) {
      lcd.setCursor(0, 0); lcd.print(L"Цель!");
    } else {
      lcd.setCursor(0, 0); lcd.print(L"На яхту");
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
          lcd.setCursor(0, 1); lcd.print(summ_money);
          coin_quantity[i]++;
          recogn_flag = true;

          checkGoal();  // проверить цель после каждой монеты
          if (goal_status == 0x01) {
            lcd.setCursor(0, 0); lcd.print(L"Цель!");
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

    while (!digitalRead(button)) {
      if (millis() - standby_timer > 2000) {
        lcd.clear();
        for (byte i = 0; i < coin_amount; i++) {
          lcd.setCursor(i * 3, 0); lcd.print((int)coin_value[i]);
          lcd.setCursor(i * 3, 1); lcd.print(coin_quantity[i]);
        }
      }
    }
  }
}

void good_night() {
  for (byte i = 0; i < coin_amount; i++) {
    EEPROM.updateInt(20 + i * 2, coin_quantity[i]);
  }
  sleep_flag = true;
  digitalWrite(disp_power, 0);
  digitalWrite(LEDpin, 0);
  digitalWrite(IRpin, 0);
  delay(100);
  // ВАЖНО: I2C hardware остаётся активным даже в powerDown на AVR,
  // но TWI прерывание НЕ будит из powerDown.
  // Поэтому Ard3 сначала дёргает пин 2 (INT0) -> wake_up() -> потом requestFrom.
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
}

void wake_up() {
  digitalWrite(disp_power, 1);
  digitalWrite(LEDpin, 1);
  digitalWrite(IRpin, 1);
  standby_timer = millis();
}
