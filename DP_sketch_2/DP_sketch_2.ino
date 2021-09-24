
#include <EEPROM.h>
#include <GyverButton.h>
#include "LowPower.h"

#include <Wire.h> 
#include "LCD_1602_RUS.h"


LCD_1602_RUS lcd(0x27, 16, 2); // создать дисплей
// если дисплей не работает, замени 0x27 на 0x3f


//-------НАСТРОЙКИ---------
#define radiusSignal 45 // диапазон для сигналов монет
#define coin_amount 7 // число монет, которые нужно распознать

float coin_value[coin_amount] = {5.0, 2.0, 10.0, 1.0, 0.5, 0.05, 0.1};  // стоимость монет (в порядке возрастания диаметров)

#define LED 7 // куда подключин ИК-светодиод
#define IRpow A3 // питание фототранзистора
#define IRsignal A6 // пин аналогово сигнала с фототранзистора
#define buttonHollPin 2 // пин отверстия манетоприёмника
#define lcdPow 10 // пин питания экрана
#define timeSetButHoll 2000 // время удержания монетки в отверстии
//-------НАСТРОЙКИ---------


int coin_signal[coin_amount]; // тут хранится значение сигнала для каждого размера монет
int coin_quantity[coin_amount];  // количество монет
int empty_signal; // храним уровень пустого сигнала
int sens_signal; // текущий сигнал
int last_sens_signal; // максимум
boolean coin_flag = false; // флаг на наличие монетки
int arr_signal[5]; // для сред.арифм монет
int meanArithmetic; // среднее арифметическое
unsigned long reset_timer; //таймер очистки памяти, он так же используется для принудительного выхода из калибровки
unsigned long standby_timer;
int stb_time = 15000; // время бездействия, через которое система уйдёт в сон (миллисекунды)
float summ_money = 0; // сумма монет в копилке
String currency = "RUB"; // валюта (английские буквы!!!)
bool sleep_flag = true;
unsigned long cheak_timer = 0;

GButton buttonHoll (buttonHollPin);

void setup() {
  
  // пины питания как выходы
  pinMode(LED, OUTPUT);
  pinMode(IRpow, OUTPUT);
  pinMode(IRsignal, OUTPUT);
  pinMode(lcdPow, OUTPUT);
  pinMode(buttonHollPin, INPUT_PULLUP);
  
  // подать питание на дисплей и датчик
  digitalWrite(LED, HIGH);
  digitalWrite(IRpow, HIGH);
  digitalWrite(lcdPow, HIGH);
  
  // подключить прерывание
  attachInterrupt(0, wake_up, CHANGE);
  
  buttonHoll.setTimeout(timeSetButHoll);

  // инициализация дисплея
  lcd.init();
  lcd.backlight();
  lcd.clear();  
  
  delay(2000);
  
  // 1) отображение опроного значения
  while (0) {
    sens_signal = analogRead(IRsignal);
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(L"value: "); lcd.print(sens_signal);
    delay(1000);
  }
  
  // проверка щели
  while (0) {
    buttonHoll.tick();
    if (buttonHoll.isHolded()) {
      Serial.println("Монета в отверстии");
    }
  }
  
  // проверка дисплея
  while (0) {
    lcd.setCursor(0, 0);
    lcd.print(L"привет я"); 
    lcd.setCursor(0, 1); lcd.print(L"цыфровая копилка");
  }

  // измерение и вывод границ фона на дисплей
  empty_signal = analogRead(IRsignal); // считать пустой (опорный/фоновый) сигнал empty_signal
  lcd.setCursor(0, 0); lcd.print(L"Вход: "); lcd.print(empty_signal);
  delay(1000);
  reset_timer = millis(); //сбросить таймер

  while (1) { // цикл на 3 секунды для замера и вывода границ фона на дисплей
    lcd.setCursor(0, 0); lcd.print(L"Замер Фона");
    sens_signal = analogRead(IRsignal); // считать фотодатчик
    if (sens_signal < empty_signal) {
      empty_signal = sens_signal;  // фиксируем минимальное значение фона
    }
    if (sens_signal > last_sens_signal) {
      last_sens_signal = sens_signal; // фиксируем максимальное значение фона
    }
    if (millis() - reset_timer > 3000) break; // выходим из режима замера фона по истечении времени 3 сек
  }
  
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(L"min:"); lcd.print(empty_signal); lcd.print(" "); lcd.print(L"max:"); lcd.print(last_sens_signal);
  sens_signal = (empty_signal + last_sens_signal) / 2;
  lcd.setCursor(0, 1); lcd.print(L"середина: "); lcd.print(sens_signal);
  empty_signal = sens_signal;
  delay(2000);

  // если монетка замыкает контакты
  if (!digitalRead(buttonHollPin)) {
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print(L"Сервис");
    delay(500);
    reset_timer = millis();
    while (1) {
      if (digitalRead(buttonHollPin)) {   // если отпустили кнопку, то просто сбросить сумму монет
        for (byte i = 0; i < coin_amount; i++) {
          coin_quantity[i] = 0;
          EEPROM.put(40 + i * 2, 0);
        }
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(L"Пamять oчищeнa");
        delay(1200);
        break;
      }
      if (millis() - reset_timer > 3500) { // отчистить память и произвести калибровку
        for (byte i = 0; i < coin_amount; i++) {
          coin_quantity[i] = 0;
          EEPROM.put(40 + i * 2, 0);
        }
        lcd.clear();
        lcd.setCursor(3, 0); lcd.print(L"Кaлибpoвкa");
        delay(1200);

        ///*  кидаем 1 раз  
        while (0) {         
          for (byte i = 0; i < coin_amount; i++) {
            lcd.clear();
            lcd.setCursor(0, 0); lcd.print(L"кaлибpуeм: "); lcd.print(coin_value[i]);
            last_sens_signal = empty_signal;
            while (1) {
              sens_signal = analogRead(IRsignal); // считать датчик
              if (sens_signal > last_sens_signal) last_sens_signal = sens_signal; // если текущее значение больше предыдущего
              if (sens_signal - empty_signal > 15) coin_flag = true; // если значение упало почти до "пустого", считать что монета улетела
              if (coin_flag && (abs(sens_signal - empty_signal)) < 4) {
                coin_signal[i] = last_sens_signal;
                EEPROM.put(i * 2, coin_signal[i]);
                lcd.setCursor(0, 1); lcd.print("                "); // удалить всё во 2-ой строчке
                lcd.setCursor(0, 1); lcd.print(L"значение: "); lcd.print(coin_signal[i]);
                coin_flag = false;
                delay(1200);
                break;          
              }
            }
          }
          lcd.clear();
          lcd.setCursor(0, 0); lcd.print(L"Калиб пройдена");
          break;
          //*/
        }
        
        // калибровка (кинуть 5 раз монет 1 типа и получить среднее арифметические из всех значенй)
        while (1) {
          for (byte i = 0; i < coin_amount; i++) {
            lcd.clear();
            lcd.setCursor(0, 0); lcd.print(L"Калибруем: "); lcd.print(coin_value[i]); lcd.print(" ");
            meanArithmetic = 0; // скипаем среднее
            for (int j = 0; j < 5; j++) {
              lcd.setCursor(13, 1); lcd.print(L"5/"); lcd.print(j); 
              delay(500);
              last_sens_signal = empty_signal;
              while (1) {
                  sens_signal = analogRead(IRsignal); // считать датчик
                  if (sens_signal > last_sens_signal) last_sens_signal = sens_signal;  // если текущее значение больше предыдущего
                  if (sens_signal - empty_signal > 15) coin_flag = true; // если значение упало почти до "пустого", считать что монета улетела
                  if (coin_flag && (abs(sens_signal - empty_signal)) < 4) {  // если монета точно улетела
                    lcd.setCursor(0, 1); lcd.print("                ");
                    lcd.setCursor(0, 1); lcd.print(L"пик: "); lcd.print(last_sens_signal);
                    arr_signal[j] = last_sens_signal;
                    coin_flag = false;
                    break;
                  }
                }
              }
              lcd.setCursor(13, 1); lcd.print(L"5/5");
              delay(1500);
              // среднее арифметическое
              for (byte f = 0; f < 5; f++) { 
                meanArithmetic += arr_signal[f];
              }
              meanArithmetic = meanArithmetic / 5;
              lcd.clear();
              lcd.setCursor(0, 1); lcd.print(L"cpeднee: "); lcd.print(meanArithmetic);
              coin_signal[i] = meanArithmetic;
              EEPROM.put(i * 2, coin_signal[i]);
              delay(2000);
            }
            lcd.clear();
            lcd.setCursor(3, 0); lcd.print(L"Кaлuбpoвкa");
            lcd.setCursor(4, 1); lcd.print(L"Пpoйдeнa");
            break;
          }
         break;
        }
      }
    }
    
  // при старте системы считать из памяти сигналы монет для дальнейшей работы, а также их количество в банке
  for (byte i = 0; i < coin_amount; i++) {
    EEPROM.get(i * 2, coin_signal[i]);
    EEPROM.get(40 + i * 2, coin_quantity[i]);
    summ_money += coin_quantity[i] * coin_value[i];  // ну и сумму сразу посчитать, как произведение цены монеты на количество
  }
  
  standby_timer = millis();  // обнулить таймер ухода в сон
}

void loop() {

  if (sleep_flag) {  // если проснулись  после сна, инициализировать дисплей и вывести текст, сумму и валюту
    delay(500);
    lcd.init();
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(L"На яхту ъ");
    lcd.setCursor(0, 1); lcd.print(summ_money); lcd.print(" "); lcd.print(currency);
    empty_signal = analogRead(IRsignal);
    sleep_flag = false;
    standby_timer = millis();
  }
  
  last_sens_signal = empty_signal; // последний сигнал равен начальному
  while (1) { // Основной цикл
    buttonHoll.tick();    
    sens_signal = analogRead(IRsignal);  // далее такой же алгоритм, как при калибровке
    if (sens_signal > last_sens_signal) last_sens_signal = sens_signal;
    if (sens_signal - empty_signal > 15) coin_flag = true;
    if (coin_flag && (abs(sens_signal - empty_signal)) < 4) {
      // в общем нашли максимум для пролетевшей монетки, записали в last_sens_signal
      // далее начинаем сравнивать со значениями для монет, хранящимися в памяти
      for (byte i = 0; i < coin_amount; i++) {
        int delta = abs(last_sens_signal - coin_signal[i]); // вот самое главное! ищем АБСОЛЮТНОЕ (то бишь по модулю)
        if (delta < radiusSignal) {  // и вот тут если эта разность попадает в диапазон, то считаем монетку распознанной
          summ_money += coin_value[i]; // к сумме тупо прибавляем цену монетки (дада, сумма считается двумя разными способами. При старте системы суммой всех монет, а тут прибавление
          coin_quantity[i]++;  // для распознанного номера монетки прибавляем количество
          lcd.clear();
          lcd.setCursor(0, 0); lcd.print(L"На яхту ъ");
          lcd.setCursor(0, 1); lcd.print(summ_money); lcd.print(" "); lcd.print(currency);
//          lcd.setCursor(0, 1); lcd.print("                 ");
//          lcd.setCursor(0, 1); lcd.print(L"Это: "); lcd.print(coin_value[i]);
          break;
        }
      }
      coin_flag = false;
      standby_timer = millis();  // сбросить таймер
      delay(500);
      break;
    }
    
    // если монетка вставлена (замыкает контакты) и удерживается 2 секунды
    if (buttonHoll.isHolded()) {
      lcd.clear();
      // отобразить на дисплее: сверху цены монет (округлено до целых!!!!), снизу их количество
      lcd.setCursor(0, 0); lcd.print("50  "); lcd.print("1  "); lcd.print("2  "); lcd.print("5  "); lcd.print("10");
      lcd.setCursor(1, 1); lcd.print(coin_quantity[4]); // 50
      lcd.setCursor(4, 1); lcd.print(coin_quantity[3]); // 1
      lcd.setCursor(7, 1); lcd.print(coin_quantity[1]); // 2
      lcd.setCursor(10, 1); lcd.print(coin_quantity[0]); // 5 
      lcd.setCursor(13, 1); lcd.print(coin_quantity[2]); // 10
    }
    
    // если ничего не делали, времят аймера вышло, спим
    if (millis() - standby_timer >= stb_time) {
      good_night();
      break;
    }    
    
  }
}

// функция сна
void good_night() {
  // перед тем как пойти спать, записываем в EEPROM новые полученные количества монет по адресам начиная с 20го (пук кек)
  for (byte i = 0; i < coin_amount; i++) {
    EEPROM.put(40 + i * 2, coin_quantity[i]);
  }
  sleep_flag = true;
  // вырубить питание со всех дисплеев и датчиков
  digitalWrite(LED, LOW);
  digitalWrite(IRpow, LOW);
  digitalWrite(lcdPow, LOW);
  delay(100);
  
  // и вот теперь спать
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
}

// просыпаемся по ПРЕРЫВАНИЮ (эта функция - обработчик прерывания)
void wake_up() {
  // возвращаем питание на дисплей и датчик
  digitalWrite(LED, HIGH);
  digitalWrite(IRpow, HIGH);
  digitalWrite(lcdPow, HIGH);
  standby_timer = millis();  // и обнуляем таймер // а может обнуляь в slip???
}
