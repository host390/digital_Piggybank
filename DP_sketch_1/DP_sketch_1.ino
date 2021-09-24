

#include <GyverButton.h>


/*
  Пошаговое описание
  1) Выполнить проверку #1
  2) Выполнить проверку #2
  3) Выполнить упрощённую калибровку монет
  если всё ок, то едем дальше :)
*/

//-------НАСТРОЙКИ---------
#define radiusSignal 45 // диапазон для сигналов монет

#define coin_amount 7 // число монет, которые нужно распознать
float coin_value[coin_amount] = {5.0, 2.0, 10.0, 1.0, 0.5, 0.05, 0.1};  // стоимость монет (в порядке возрастания диаметров)

#define LED 7 // куда подключин ИК-светодиод
#define IRpow A3 // питание фототранзистора
#define IRsignal A6 // пин аналогово сигнала с фототранзистора
//-------НАСТРОЙКИ---------

int coin_signal[coin_amount]; // тут хранится значение сигнала для каждого размера монет
int empty_signal; // храним уровень пустого сигнала
int sens_signal; // текущий сигнал
int last_sens_signal; // максимум
boolean coin_flag = false; // флаг на наличие монетки
int arr_signal[5]; // для сред.арифм монет
int meanArithmetic; // среднее арифметическое
unsigned long reset_timer; //таймер очистки памяти, он так же используется для принудительного выхода из калибровки
unsigned long standby_timer;

void setup() {
  Serial.begin(9600); // открыть порт для связи с ПК для отладки
  delay(500);
  // пины питания как выходы
  pinMode(LED, OUTPUT);
  pinMode(IRpow, OUTPUT);
  pinMode(IRsignal, OUTPUT);

  // подать питание на дисплей и датчик
  digitalWrite(LED, HIGH);
  digitalWrite(IRpow, HIGH);
  delay(2000);
  Serial.println("замер фона ...");

  // измерение и вывод границ фона на дисплей
  empty_signal = analogRead(IRsignal);  // считать пустой (опорный/фоновый) сигнал empty_signal
  reset_timer = millis(); //сбросить таймер

  while (1) {// цикл на 3 секунды для замера и вывода границ фона на дисплей
    sens_signal = analogRead(IRsignal); // считать фотодатчик
    if (sens_signal < empty_signal) {
      empty_signal = sens_signal;  // фиксируем минимальное значение фона
    }
    if (sens_signal > last_sens_signal) {
      last_sens_signal = sens_signal; // фиксируем максимальное значение фона
    }
    if (millis() - reset_timer > 3000) break; // выходим из режима замера фона по истечении времени 3 сек
  }
  
  Serial.print("Минимальный опорный сигнал: ");
  Serial.println(empty_signal);
  Serial.print("Максимальный опорный сигнал: ");
  Serial.println(last_sens_signal);

  sens_signal = (empty_signal + last_sens_signal) / 2;
  Serial.print("Середина: ");
  Serial.println(sens_signal);
  empty_signal = sens_signal;
  delay(2000);// время отображения фона, после чего переходим непосредственно к калибровке

  // 1) Первая проверка
  while (0) {
    sens_signal = analogRead(IRsignal);
    Serial.print("value: ");
    Serial.println(sens_signal);
  }

  // 2) Вторая проверка
  while (1) {
    for (byte i = 0; i < coin_amount; i++) {
      Serial.print("Калибруем: ");
      Serial.println(coin_value[i]);
      last_sens_signal = empty_signal;
      while (1) {
        sens_signal = analogRead(IRsignal); // считать датчик
        if (sens_signal > last_sens_signal) last_sens_signal = sens_signal;  // если текущее значение больше предыдущего
        if (sens_signal - empty_signal > 15) coin_flag = true; // если значение упало почти до "пустого", считать что монета улетела
        if (coin_flag && (abs(sens_signal - empty_signal)) < 4) {            // если монета точно улетела
          coin_signal[i] = last_sens_signal;                                 // записать максимальное значение в память
          Serial.print("Значение ");
          Serial.print(coin_value[i]);
          Serial.print(": ");
          Serial.println(coin_signal[i]);
          coin_flag = false;
          break;
        }
      }

    }
    Serial.println("\nКалибровка пройдена");
    Serial.println("Таблица калибровки #1: ");
    Serial.println("------------------------------------");
    for (byte i = 0; i < coin_amount; i++) {
      Serial.print(coin_value[i]);
      Serial.print(" ----> ");
      Serial.println(coin_signal[i]);
      Serial.println("------------------------------------");
    }
    break;
  }

  // Упрощённая калибровка монеток
  while (1) {
    for (byte i = 0; i < coin_amount; i++) {
      Serial.print("Калибруем: ");
      Serial.println(coin_value[i]);
      meanArithmetic = 0; // скипаем среднее
      for (byte j = 0; j < 5; j++) {
        delay(500);
//        empty_signal = analogRead(IRsignal);
//        Serial.print("Новый опорный сигнал: ");
//        Serial.println(empty_signal);

        last_sens_signal = empty_signal;
        Serial.print("(Киньте монету 5 раз)");
        Serial.print(" 5/");
        Serial.println(j);

        while (1) {
          sens_signal = analogRead(IRsignal); // считать датчик
          if (sens_signal > last_sens_signal) last_sens_signal = sens_signal;  // если текущее значение больше предыдущего
          if (sens_signal - empty_signal > 15) coin_flag = true; // если значение упало почти до "пустого", считать что монета улетела
          if (coin_flag && (abs(sens_signal - empty_signal)) < 4) {  // если монета точно улетела
            Serial.print("Сигнал: ");
            Serial.println(last_sens_signal);
            arr_signal[j] = last_sens_signal;
            coin_flag = false;
            break;
          }
        }
      }
      Serial.print("Все значения ");
      Serial.print(coin_value[i]);
      Serial.print(": ");
      for (byte f = 0; f < 5; f++) {
        meanArithmetic += arr_signal[f];
        Serial.print(arr_signal[f]);
        if (f != 4) Serial.print(",");
      }
      meanArithmetic = meanArithmetic / 5;
      Serial.print("\nСреднее арифметическое: ");
      Serial.println(meanArithmetic);
      coin_signal[i] = meanArithmetic;
    }
    Serial.println("\nКалибровка пройдена");
    Serial.println("Таблица калибровки #2: ");
    Serial.println("------------------------------------");
    for (byte i = 0; i < coin_amount; i++) {
      Serial.print(coin_value[i]);
      Serial.print(" ----> ");
      Serial.println(coin_signal[i]);
      Serial.println("------------------------------------");
    }
    break;
  }
}

void loop() {
  delay(500);
  empty_signal = analogRead(IRsignal);
  Serial.print("Новый опорный сигнал: ");
  Serial.println(empty_signal);
  last_sens_signal = empty_signal; // последний сигнал равен начальному

  // Основной цикл
  while (1) {    
    sens_signal = analogRead(IRsignal);  // далее такой же алгоритм, как при калибровке
    if (sens_signal > last_sens_signal) last_sens_signal = sens_signal;
    if (sens_signal - empty_signal > 15) coin_flag = true;
    if (coin_flag && (abs(sens_signal - empty_signal)) < 4) {
      // в общем нашли максимум для пролетевшей монетки, записали в last_sens_signal
      // далее начинаем сравнивать со значениями для монет, хранящимися в памяти
      Serial.print("Ещё сигнал: ");
      Serial.println(last_sens_signal);
      for (byte i = 0; i < coin_amount; i++) {
        int delta = abs(last_sens_signal - coin_signal[i]);   // вот самое главное! ищем АБСОЛЮТНОЕ (то бишь по модулю)
        // значение разности полученного сигнала с нашими значениями из памяти
        if (delta < radiusSignal) {
          Serial.print("Это: ");
          Serial.print(coin_value[i]);
          Serial.print(" макс. сигнал: ");
          Serial.println(last_sens_signal);
          break;
        }
      }
      coin_flag = false;
      break;
    }
  }
}
