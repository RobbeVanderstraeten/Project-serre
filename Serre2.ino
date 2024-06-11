#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define SS_PIN 4
#define RST_PIN 25
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance.

#include "DHT.h"
#define DHTPIN 33
#define DHTTYPE DHT11  // DHT 11
DHT dht(DHTPIN, DHTTYPE);

#include <OneWire.h>
#include <DallasTemperature.h>
const int oneWireBus = 32;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

#include <LiquidCrystal_I2C.h>
int lcdColumns = 16;
int lcdRows = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

//////////////////////////////////////////

const char* ssid = "nieuweserre";
const char* password = "Ferrefien";

// MQTT instellingen
const char* mqtt_server = "192.168.0.200";
const int mqtt_port = 1883;
const char* mqttUser = "robbe";
const char* mqttPassword = "VDS1";

WiFiClient espClient;
PubSubClient client(espClient);

const int pomp = 16;  //pomp
const int vent = 13;  //vent
const int trigPin = 27;
const int echoPin = 26;
long duration, cm;

float LT;  //lucht temp
float LV;  //lucht Hum
float BT;  //bodem Temp
float BV;  //bodem Hum

float WP;  //water pomp status
float V;   //ventilator status

float ds18b20 = sensors.getTempCByIndex(0);
float BVsensor = map(analogRead(35), 0, 4095, 100, 0);

void setup() {
  Serial.begin(115200);  // Initiate a serial communication

  pinMode(BVsensor, INPUT);
  pinMode(pomp, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  lcd.init();
  lcd.backlight();
  SPI.begin();         // Initiate  SPI bus
  mfrc522.PCD_Init();  // Initiate MFRC522
  dht.begin();
  sensors.begin();

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(5000);
    Serial.println("Verbinding maken met WiFi...");
  }

  Serial.println("Verbonden met WiFi");
  Serial.print("IP-adres: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, mqtt_port);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  sensors.requestTemperatures();
  float ds18b20 = sensors.getTempCByIndex(0);
  float BVsensor = map(analogRead(35), 0, 4095, 100, 0);

  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  pinMode(echoPin, INPUT);
  duration = pulseIn(echoPin, HIGH);

  cm = (duration / 2) / 29.1;

  float afstand = map(cm, 8, 28, 100, 0);

  if (cm < 22) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(dht.readHumidity());
    lcd.print(" %");

    lcd.setCursor(9, 0);
    lcd.print(dht.readTemperature());
    lcd.print(" C");

    lcd.setCursor(0, 1);
    lcd.print(BVsensor);
    lcd.print(" %");

    lcd.setCursor(9, 1);
    lcd.print(ds18b20);
    lcd.print(" C");

  } else if (cm > 22) {
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("Refill Water!");
    delay(2000);
  }

  Serial.println(dht.readHumidity());
  Serial.println(dht.readTemperature());
  Serial.println(BVsensor);
  Serial.println(ds18b20);
  Serial.print(afstand);
  Serial.print(" %  ");
  Serial.print(cm);
  Serial.println(" cm");

  String payload1 = String(dht.readTemperature());
  String payload2 = String(dht.readHumidity());
  String payload3 = String(ds18b20);
  String payload4 = String(BVsensor);
  String payload5 = String(LT);
  String payload6 = String(LV);
  String payload7 = String(BT);
  String payload8 = String(BV);
  String payload9 = String(V);
  String payload10 = String(WP);
  String payload11 = String(afstand);

  client.publish("serre/dht11/Temperature", payload1.c_str());
  client.publish("serre/dht11/Humidity", payload2.c_str());
  client.publish("serre/ds18b20/bodemtemp", payload3.c_str());
  client.publish("serre/BVsensor/bodemvocht", payload4.c_str());
  client.publish("serre/LT/Gevraagde lucht temp", payload5.c_str());
  client.publish("serre/LV/Gevnraagde lucht hum", payload6.c_str());
  client.publish("serre/BT/gevraagde bodem temp", payload7.c_str());
  client.publish("serre/BV/gevraagde bodem hum", payload8.c_str());
  client.publish("serre/Ventilator/Status vent", payload9.c_str());
  client.publish("serre/Water pomp/Status pomp", payload10.c_str());
  client.publish("serre/Water tank/Water niveau", payload11.c_str());
  delay(3000);

  if (BVsensor < BV - 3) {  //water pomp
    WP = 1;
    digitalWrite(pomp, HIGH);
  }
  if (BVsensor > BV + 3) {
    WP = 0;
    digitalWrite(pomp, LOW);
  }

  if (dht.readTemperature() > LT + 3) {  //Ventilator
    V = 1;
    digitalWrite(vent, HIGH);
  }
  if (dht.readTemperature() < LT - 3) {
    V = 0;
    digitalWrite(vent, LOW);
  }


  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  //Show UID on serial monitor
  Serial.print("UID tag :");
  String content = "";
  byte letter;
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }

  Serial.println();
  Serial.println("Message : ");
  content.toUpperCase();

  if (content.substring(1) == "63 3F 55 14") {  //Tijm
    LT = 30;
    LV = 65;
    BT = 20;
    BV = 0;
  }
  if (content.substring(1) == "A3 37 CE 26") {  //Paprika
    LT = 10;
    LV = 65;
    BT = 20;
    BV = 50;
  }

  Serial.println(LT);
  Serial.println(LV);
  Serial.println(BT);
  Serial.println(BV);
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Verbinding maken met MQTT Broker...");

    if (client.connect("DHT11Client", mqttUser, mqttPassword)) {
      Serial.println("Verbonden met MQTT Broker");
    } else {
      Serial.print("Fout bij het verbinden met MQTT Broker. Foutcode: ");
      Serial.print(client.state());
      Serial.println(" Opnieuw proberen");
    }
  }
}
