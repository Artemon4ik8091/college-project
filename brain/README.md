# модуль верификации и команды списания

проверка хеша, полученного от `reader`, и отправка команды на разрешение или отказ списания

## компоненты

- arduino nano
- arduino nano `reader`

## распиновка
```
reader a4 (sda)   <->  receiver a4 (sda)
reader a5 (scl)   <->  receiver a5 (scl)
reader gnd        <->  receiver gnd
```
