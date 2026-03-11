# Модуль оплаты картой
Эмуляция успешной/не успешной оплаты банковской картой.
## Компоненты
- RFID Reader RC522
- Arduino Nano
- LED RGB K851264
## Распиновка
#### Arduino Nano <-> RFID Reader
```
3.3v <-> 3.3v
D9 <-> RST
GND <-> GND
D12 <-> MISO
D11 <-> MOSI
D13 <-> SCK
D10 <-> SDA
```
#### Arduino Nano (Модуль оплаты/reader) <-> Arduino Nano (Основные мозги/receiver)
```
reader a4 (sda)   <->  receiver a4 (sda)
reader a5 (scl)   <->  receiver a5 (scl)
reader gnd        <->  receiver gnd
```
#### Arduino Nano <-> LED RGB K851264 || 
> ***Временное решение!***
```
GND <-> GND
D2 <-> R
D3 <-> G
```
