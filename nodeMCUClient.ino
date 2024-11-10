#include <GyverBME280.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

// Инициализация объектов и определений
GyverBME280 bme;

// Константы для температуры и влажности
const float DAY_TEMP_LOW = 25.0;                   // Включение обогрева днём
const float DAY_TEMP_HIGH = 27.0;                  // Выключение обогрева днём
const float NIGHT_TEMP_LOW = 19.0;                 // Включение обогрева ночью
const float NIGHT_TEMP_HIGH = 21.0;                // Выключение обогрева ночью
const float DAY_HUMIDITY = 55.0;                   // Влажность днём
const float NIGHT_HUMIDITY = 45.0;                 // Влажность ночью
const int analogPin = A0;                          // Определение пина для аналогового датчика
const int relayPins[] = { 0, 2, 14, 12, 13, 15 };  // Массив пинов реле
const unsigned long SYNC_RETRY_INTERVAL = 60000;   // 60 seconds
unsigned long lastSyncAttempt = 0;

// Глобальная переменная для состояния обогрева
static bool heatingState = false;

// Глобальные переменные для хранения значений датчиков
float temperature, humidity, pressure, uvIndex;

// Настройки сети
const char* ssid = "ssid";
const char* password = "password";
const char* serverHost = "http://www.kotikbank.ru/";

// NTP клиент
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

void setup() {
  // Инициализация последовательного порта для отладки
  Serial.begin(9600);
  // Инициализация датчика BME280
  if (!bme.begin(0x76)) {
    Serial.println("Ошибка инициализации датчика BME280!");
    return;
  }
  // Инициализация пинов реле как выходов
  for (int pin : relayPins) {
    pinMode(pin, OUTPUT);
  }
  // Подключение к Wi-Fi сети
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Подключение к Wi-Fi...");
    delay(5000);
  }
  Serial.print("IP-адрес: ");
  Serial.println(WiFi.localIP());

  // Инициализация NTP клиента и синхронизация времени
  timeClient.begin();
  timeClient.update();
  setTime(timeClient.getEpochTime());  // Установка времени с помощью NTP

  // Установление начального состояния реле
  digitalWrite(relayPins[0], HIGH);
  digitalWrite(relayPins[1], LOW);
  digitalWrite(relayPins[2], HIGH);
  digitalWrite(relayPins[3], LOW);
  digitalWrite(relayPins[4], HIGH);
  digitalWrite(relayPins[5], LOW);
}

void loop() {
  if (!timeClient.update()) {
    if (millis() - lastSyncAttempt > SYNC_RETRY_INTERVAL) {
      timeClient.forceUpdate();
      lastSyncAttempt = millis();
    }
    Serial.println("Ожидание синхронизации...");
    delay(1000);
    return;  // Выходим из loop, если время не синхронизировано
  }

  // Обновляем время в каждом цикле
  setTime(timeClient.getEpochTime());

  // Вывод данных на последовательный порт
  sensorData();

  // Управление реле на основе значений датчиков
  manageRelays();

  // Отправка данных на сервер
  sendDataToServer();
}

// Функция для вывода данных датчиков
void sensorData() {
  // Чтение значения с аналогового датчика (предположим, UV-датчик)
  uvIndex = analogRead(analogPin);

  // Чтение данных с датчика BME280
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0;
  // Проверка диапазонов (пример, реальные диапазоны зависят от конкретных датчиков)
  if (temperature < -50 || temperature > 100 || humidity < 0 || humidity > 100 || pressure < 300 || pressure > 1100 || uvIndex < 0) {
    Serial.println("Ошибка чтения сенсоров!");
  }

  Serial.print("Температура: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Влажность: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Давление: ");
  Serial.print(pressure);
  Serial.println(" hPa");

  Serial.print("Индекс UV: ");
  Serial.println(uvIndex);
  Serial.println();
}

void manageRelays() {
  // Получаем текущий час и минуту
  int currentHour = hour();
  int currentMinute = minute();

  // Проверяем день или ночь (7:00 - 23:00)
  bool isDay = (currentHour > 7 || (currentHour == 7 && currentMinute >= 0)) && 
               (currentHour < 23 || (currentHour == 23 && currentMinute < 60));

  // Определяем пороги для текущего времени суток
  float tempLowThreshold = isDay ? DAY_TEMP_LOW : NIGHT_TEMP_LOW;
  float tempHighThreshold = isDay ? DAY_TEMP_HIGH : NIGHT_TEMP_HIGH;

  // Обновляем состояние обогрева с использованием гистерезиса
  heatingState = !heatingState ? (temperature < tempLowThreshold) : 
                                (temperature <= tempHighThreshold);

  // Определяем необходимость увлажнения
  bool humidifying = humidity < (isDay ? DAY_HUMIDITY : NIGHT_HUMIDITY);

  // Проверка критической температуры
  turnOffAllRelays();

  // Управление реле
  controlRelayPair(relayPins[4], relayPins[5], isDay, "Управление светом", "Реле 4 и 5");
  controlRelayPair(relayPins[0], relayPins[1], heatingState, "Управление обогревом", "Реле 0 и 1");
  controlRelayPair(relayPins[2], relayPins[3], humidifying, "Управление увлажнителем", "Реле 2 и 3");
}

// Функция для управления парой реле
void controlRelayPair(int pin1, int pin2, bool state, const char* reason, const char* relayName) {
  // Попытка включить/выключить первое реле
  digitalWrite(pin1, state);
  delay(100);  // Небольшая задержка для стабилизации

  // Проверка состояния первого реле
  if (digitalRead(pin1) == state) {
    Serial.print(reason);
    Serial.print(" для ");
    Serial.print(relayName);
    Serial.println(state ? " включено" : " отключено");
  } else {
    // Если первое реле не удалось включить/выключить, работаем с запасным реле
    digitalWrite(pin2, state);
    delay(100);  // Небольшая задержка для стабилизации

    if (digitalRead(pin2) == state) {
      Serial.print(reason);
      Serial.print(" для запасного ");
      Serial.print(relayName);
      Serial.println(state ? " включено" : " отключено");
    } else {
      Serial.print("Не удалось ");
      Serial.print(state ? "включить" : "отключить");
      Serial.print(" ни основное, ни запасное ");
      Serial.print(relayName);
      Serial.println("!");
    }
  }
}

// Функция для отключения всех реле
void turnOffAllRelays() {
  if (temperature >= 38 || temperature <= 5) {
    for (int pin : relayPins) {
      digitalWrite(pin, LOW);
    }
    Serial.println("Критическая температура! Все реле отключены.");
    return;
  }
}

// Функция для отправки данных на сервер
void sendDataToServer() {
  // Формирование запроса к серверу
  String params = String(
    "?method=GAoYgAQyEggEEC4YChjHARixAxjRA&rele1=" + String(digitalRead(relayPins[0]) == HIGH) + "&rele2=" + String(digitalRead(relayPins[1]) == HIGH) + "&rele3=" + String(digitalRead(relayPins[2]) == HIGH) + "&rele4=" + String(digitalRead(relayPins[3]) == HIGH) + "&rele5=" + String(digitalRead(relayPins[4]) == HIGH) + "&rele6=" + String(digitalRead(relayPins[5]) == HIGH) + "&temperature=" + String(temperature) + "&pressure=" + String(pressure) + "&humidity=" + String(humidity) + "&uv_index=" + String(uvIndex));

  // Отправка данных на сервер
  WiFiClient client;
  HTTPClient http;
  String url = serverHost + params;
  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("Ответ сервера: " + response);

    // Парсинг JSON
    DynamicJsonDocument jsonDoc(2048);
    DeserializationError error = deserializeJson(jsonDoc, response);

    if (!error) {
      // Обработка параметров
      unsigned long unixTime = jsonDoc["time"].as<int>();
      int emergencyShutdown = jsonDoc["emergencyShutdown"].as<int>();

      if (emergencyShutdown == 1) {
        turnOffAllRelays();
        Serial.println("Ручное аварийное отключение реле!");
      } else if (emergencyShutdown == 0) {
        Serial.println("Нормальная работа, ничего не делаем.");
      } else {
        Serial.println("Некорректное значение emergencyShutdown!");
      }
    } else {
      Serial.print("Ошибка парсинга JSON: ");
      Serial.println(error.f_str());
    }
  } else {
    Serial.println("httpCode: " + String(httpCode));
  }

  Serial.println(url);
  http.end();
}
