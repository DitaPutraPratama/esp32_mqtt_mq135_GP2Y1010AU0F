#include <Arduino.h>            
#include <WiFi.h>               
#include <PubSubClient.h>       
#include <ArduinoJson.h>        
#include <GP2YDustSensor.h>     
#include <MQUnifiedsensor.h>    
#include <LiquidCrystal_I2C.h>  
#include <math.h>

#define placa "ESP-32"
#define Voltage_Resolution 3.3
#define pin 35
#define type "MQ-135"
#define ADC_Bit_Resolution 12
#define RatioMQ135CleanAir 3.6

#define WIFI_SSID "your ssid wifi"       
#define WIFI_PASSWORD "your password"  

const uint8_t SHARP_LED_PIN = 16;
const uint8_t SHARP_VO_PIN = 36;
const float molarMassCO = 28.01;      
const float standardVolume = 22.414; 

const int lcdColumns = 16;
const int lcdRows = 2;
const int buzzer = 23;

LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);
GP2YDustSensor dustSensor(GP2YDustSensorType::GP2Y1014AU0F, SHARP_LED_PIN, SHARP_VO_PIN);
MQUnifiedsensor MQ135(placa, Voltage_Resolution, ADC_Bit_Resolution, pin, type);

const char* mqttBroker = "test.mosquitto.org"; //sesuaikan broker yang digunakan, saya menggunakan mosquitto mqtt

WiFiClient espClient;          
PubSubClient mqtt(espClient);  


void connectToWifi();  
void connectMQTT();    

void callback(char* topic, byte* payload, unsigned int length) {  
  Serial.print("Message received on topic: ");                    
  Serial.println(topic);                                          

  Serial.print("Message: ");                   
  for (unsigned int i = 0; i < length; i++) {  
    Serial.print((char)payload[i]);           
  }
  Serial.println();  

  digitalWrite(buzzer, LOW);
  String statusIspu;

  switch ((char)payload[0]) {
    case '1':
      statusIspu = "Baik";
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Status: ");
      lcd.setCursor(0, 1);
      lcd.print(statusIspu);
      delay(1500);
      break;
    case '2':
      statusIspu = "Sedang";
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Status: ");
      lcd.setCursor(0, 1);
      lcd.print(statusIspu);
      delay(1500);
      break;
    case '3':
      statusIspu = "Tidak Sehat";
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Status: ");
      lcd.setCursor(0, 1);
      lcd.print(statusIspu);
      
      for (int i = 0; i < 5; i++) {
        digitalWrite(buzzer, HIGH);
        delay(500);
        digitalWrite(buzzer, LOW);
        delay(500);
      }
      delay(500);
      break;
    case '4':
      statusIspu = "Sangat Tidak Sehat";
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Status: ");
      lcd.setCursor(0, 1);
      lcd.print(statusIspu);
      for (int i = 0; i < 5; i++) {
        digitalWrite(buzzer, HIGH);
        delay(1500);
        digitalWrite(buzzer, LOW);
        delay(1500);
      }
      delay(500);
      break;
    case '5':
      statusIspu = "Berbahaya";
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Status: ");
      lcd.setCursor(0, 1);
      lcd.print(statusIspu);
      digitalWrite(buzzer, HIGH);
      delay(10000);
      digitalWrite(buzzer, LOW);
      delay(500);
      break;
    default:
      statusIspu = "Unknown";
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Status: ");
      lcd.setCursor(0, 1);
      lcd.print(statusIspu);
      delay(500);
      break;
  }
}

void setup() {
  Serial.begin(9600);  

  lcd.init();                              
  lcd.backlight();                         
  pinMode(buzzer, OUTPUT);                 
  digitalWrite(buzzer, LOW);               
  dustSensor.setBaseline(0.4);             
  dustSensor.setCalibrationFactor(0.057);  
  MQ135.setRegressionMethod(1);            

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print("WiFi...");
  connectToWifi();                   
  mqtt.setServer(mqttBroker, 1883);  
  mqtt.setCallback(callback);        

  dustSensor.begin();
  MQ135.init();

  
  Serial.print("Calibrating MQ-135...");
  float calcR0 = 0;
  for (int i = 0; i < 10; i++) {
    MQ135.update();                                 
    calcR0 += MQ135.calibrate(RatioMQ135CleanAir);  
    Serial.print(".");
    delay(1000);  
  }
  MQ135.setR0(calcR0 / 10);  
  Serial.println(" done!");

  
  if (isinf(calcR0)) {
    Serial.println("Warning: R0 is infinite. Check wiring and supply.");
    while (true)
      ;
  }
  if (calcR0 == 0) {
    Serial.println("Warning: R0 is zero. Check wiring and supply.");
    while (true)
      ;
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {  
    connectToWifi();                    
  }

  if (!mqtt.connected()) {  
    connectMQTT();         
  }

  float dustDensityUG = dustSensor.getDustDensity();
  float dustDensityMG = dustDensityUG / 1000.0;

  
  MQ135.update();
  MQ135.setA(605.18);
  MQ135.setB(-3.937);
  float COppm = MQ135.readSensor();
  float COmgm3 = COppm * molarMassCO / 24.45;
  float COmug3 = COmgm3 * 1000.0;

  //kode yang dikomen ini digunakan jika kamu belum memasang sensor pada ESP 32

  // DynamicJsonDocument doc(200); 
  // doc["temp"] = String(random(0, 120)); 
  // doc["hum"] = String(random(0, 120)); 

  DynamicJsonDocument doc(200);
  doc["co"] = COmug3;
  doc["pm25"] = dustDensityUG;

  String sensorValue;               
  serializeJson(doc, sensorValue);  

  mqtt.publish("topik/topik_spesifik_esp", sensorValue.c_str()); //ubah topik sesuai kebutuhan
  mqtt.loop();                                     

  Serial.println("");
  Serial.print("pm25: ");
  Serial.print(dustDensityUG);
  Serial.println(" ug/m³");
  Serial.print("CO: ");
  Serial.print(COmug3);
  Serial.println(" ug/m³");
  lcd.clear();          
  lcd.setCursor(0, 0);  
  lcd.print("CO: ");
  lcd.print((int)round(COmug3));  
  lcd.print(" ug/m3");
  lcd.setCursor(0, 1);  
  lcd.print("PM2.5: ");
  lcd.print((int)round(dustDensityUG));  
  lcd.print(" ug/m3");
  delay(3000);  
}

void connectMQTT() {
  while (!mqtt.connected()) {                 
    Serial.println("Connecting to mqtt...");  
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (mqtt.connect(clientId.c_str())) {
      Serial.println("connected");
      
      mqtt.subscribe("topik/spesifik_express"); //ubah topik sesuai kebutuhan
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);  
    }
  }
}

void connectToWifi() {
  Serial.println("Connecting to wifi");    
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);    
  while (WiFi.status() != WL_CONNECTED) {  
    Serial.print(".");                     
    delay(500);                            
  }

  Serial.println("\nWiFi Connected");  
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());  
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());  
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());  
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());  
  Serial.print("Gateway IP: ");
  Serial.println(WiFi.gatewayIP());  
  Serial.print("DNS IP: ");
  Serial.println(WiFi.dnsIP()); 
}
