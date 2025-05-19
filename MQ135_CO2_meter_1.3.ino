/*
CO2 meter v1.3 - Исправленная версия с корректным синтаксисом
*/
#include <MQUnifiedsensor.h>
#include <DHT.h>
#include <Wire.h>
#include <LCD_1602_RUS.h>

#define VERSION 1.3
// Конфигурация MQ-135
#define placa "Arduino UNO"
#define Voltage_Resolution 5
#define pin A2
#define type "MQ-135"
#define ADC_Bit_Resolution 10
//#define RatioMQ135CleanAir 3.0  // Правильное соотношение для CO2
#define RatioMQ135CleanAir 3.6//RS / R0 = 3.6 ppm 

// Конфигурация DHT
#define DHTPIN 5
#define DHT_HUMI_LAMBDA 2.5
#define DHTTYPE DHT11
#define DEBUG 1
#define LED 13
#define AVG_NUMBER 20           // Количество усреднений

DHT dht(DHTPIN, DHTTYPE);
MQUnifiedsensor MQ135(placa, Voltage_Resolution, ADC_Bit_Resolution, pin, type);
LCD_1602_RUS lcd(0x27, 16, 2);

uint8_t heart[8] = {0x0, 0xa, 0x1f, 0x1f, 0xe, 0x4, 0x0};

void setup() {
#if (DEBUG == 1)
  Serial.begin(9600);
#endif

  /*
    Exponential regression:
  GAS      | a      | b
  CO       | 605.18 | -3.937  
  Alcohol  | 77.255 | -3.18 
  CO2      | 110.47 | -2.862
  Toluen   | 44.947 | -3.445
  NH4      | 102.2  | -2.473
  Aceton   | 34.668 | -3.369
  */

  // Настройка MQ-135 для CO2
  MQ135.setRegressionMethod(1);
  MQ135.setA(110.47);
  MQ135.setB(-2.862); // Configure the equation to calculate CO2 concentration value
  
  MQ135.init();
  //If the RL value is different from 10K please assign your RL value with the following method:
  MQ135.setRL(10);
  
  // Калибровка
  Serial.print("Calibrating...");
  float calcR0 = 0;
  for (int i = 1; i <= 100; i++) {
    MQ135.update();
    calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
    Serial.print(".");
    delay(10);
  }
  MQ135.setR0(calcR0 / 100);
  Serial.println(" done!");

  // Проверки ошибок
  if (isinf(calcR0)) {
    Serial.println("Error: Infinite value - check wiring!");
    while(1);
  }
  if (calcR0 == 0) {
    Serial.println("Error: Zero value - check sensor!");
    while(1);
  }

  // Инициализация устройств
  dht.begin();
  lcd.init();
  lcd.backlight();
  lcd.createChar(3, heart);
  
  lcd.setCursor(1, 0);
  lcd.print("CO2 meter ");
  lcd.write(3); lcd.write(3); lcd.write(3);
  lcd.setCursor(2, 1);
  lcd.print("version" );
  lcd.print(VERSION);
  delay(1000);
  lcd.clear();
  
  pinMode(LED, OUTPUT);
  testLcd();
}

void loop() {
  static float ppmBuffer[AVG_NUMBER];
  static byte bufferIndex = 0;
  
  // Чтение температуры и влажности
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity() * DHT_HUMI_LAMBDA;
  
  // Чтение и усреднение значений CO2
  MQ135.update();
  float currentPPM = MQ135.readSensor();
  ppmBuffer[bufferIndex] = currentPPM;
  bufferIndex = (bufferIndex + 1) % AVG_NUMBER;
  
  // Расчет среднего
  float avgPPM = 0;
  for (byte i = 0; i < AVG_NUMBER; i++) {
    avgPPM += ppmBuffer[i];
  }
  avgPPM /= AVG_NUMBER;
  avgPPM +=400;

  // Отладка
#if (DEBUG == 1)
  Serial.print("CO2: "); Serial.print(avgPPM);
  Serial.print("ppm\tTemp: "); Serial.print(temperature);
  Serial.print("C\tHumi: "); Serial.print(humidity); Serial.println("%");
#endif

  // Вывод на дисплей
  printLcd(avgPPM, temperature, humidity);
  delay(200); 
}

void printLcd(float ppm, float temp, float humi) {
  lcd.setCursor(0, 0);
  lcd.print("CO2 ");
  lcd.print(ppm, 2); 
  lcd.print(" ppm  ");
  
  lcd.setCursor(0, 1);
  lcd.print(temp, 1);
  lcd.print("\xDF""C ");
  lcd.print(humi >= 80 ? "HI" : String(humi, 1));
  lcd.print("%  ");
}

void testLcd() {
  lcd.home();
  for (uint8_t i = 0; i < 16; i++) {
    lcd.write(0xFF);
    delay(50);
  }
  lcd.setCursor(0, 1);
  for (uint8_t i = 0; i < 16; i++) {
    lcd.write(0xFF);
    delay(50);
  }
  delay(1000);
  lcd.clear();
}

void alarma() {
  static uint32_t tmr = 0;
  static bool flg = false;
  if (millis() - tmr >= 500) {
    tmr = millis();
    flg = !flg;
    lcd.setBacklight(flg);
    digitalWrite(LED, flg);
  }
}
