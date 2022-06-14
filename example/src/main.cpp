#include <Arduino.h>
#include <IBMIOTF8266.h>
#include <DHTesp.h>
#include <LiquidCrystal_PCF8574.h>
#include <Wire.h>

String user_html = ""
// USER CODE EXAMPLE : your custom config variable 
// in meta.XXXXX, XXXXX should match to ArduinoJson index to access
    "<p><input type='text' name='meta.yourVar' placeholder='Your Custom Config'>";
                    ;
// for meta.XXXXX, this var is the C variable to hold the XXXXX
int             customVar1;
// USER CODE EXAMPLE : your custom config variable

char*               ssid_pfix = (char*)"IOTValve";
unsigned long       lastPublishMillis = - pubInterval;

LiquidCrystal_PCF8574 lcd(0x27);
DHTesp dht;

#define DHTPIN 14
#define RELAY 16 // with pump
#define REDPIN 13
#define BLUEPIN 2
#define GREENPIN 12
#define ILLUMIPIN 0

unsigned long last_pump_mil = 0;
bool pump_on = 0;
unsigned long lastDHTReadMillis = 0;
char tdata[16]; // display string
char hdata[16];
                       
int val = 0; // illumination sensor value 0~1023
float f_val = 0;
float pre_y = 0;
float ts = 0.1;
float tau = 0.5;

float h = 0;
float t = 0;

void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");

// USER CODE EXAMPLE : command handling
    data["valve"] = digitalRead(RELAY) ? "on" : "off";
// USER CODE EXAMPLE : command handling

    serializeJson(root, msgBuffer);
    client.publish(publishTopic, msgBuffer);
}

void handleUserCommand(JsonDocument* root) {
    JsonObject d = (*root)["d"];
    
// USER CODE EXAMPLE : status/change update
// code if any of device status changes to notify the change
    Serial.println(d.containsKey("valve"));
    if(d.containsKey("valve")) {
        if (strcmp(d["valve"], "on")) {
            digitalWrite(RELAY, HIGH);
            Serial.println("valve on");
        } else {
            digitalWrite(RELAY, LOW);
            Serial.println("valve off");
        }
        lastPublishMillis = - pubInterval;
    }
// USER CODE EXAMPLE
}

void message(char* topic, byte* payload, unsigned int payloadLength) {
    byte2buff(msgBuffer, payload, payloadLength);
    StaticJsonDocument<512> root;
    DeserializationError error = deserializeJson(root, String(msgBuffer));
  
    if (error) {
        Serial.println("handleCommand: payload parse FAILED");
        return;
    }

    handleIOTCommand(topic, &root);
    if (strstr(topic, "/device/update")) {
// USER CODE EXAMPLE : meta data update
// If any meta data updated on the Internet, it can be stored to local variable to use for the logic
// in cfg["meta"]["XXXXX"], XXXXX should match to one in the user_html
        customVar1 = cfg["meta"]["yourVar"];
// USER CODE EXAMPLE
    } else if (strstr(topic, "/cmd/")) {            // strcmp return 0 if both string matches
        handleUserCommand(&root);
    }
}

//filteting illumination sensor value
void filter(){
  val = analogRead(ILLUMIPIN);
  f_val = (tau*pre_y + ts*val) / (tau + ts);
  pre_y = f_val;
}

void GetTemperature() {
    unsigned long currentMillis = millis();
    if(currentMillis - lastDHTReadMillis >= 2000) {
        lastDHTReadMillis = currentMillis;
        h = dht.getHumidity(); // Read humidity (percent)
        t = dht.getTemperature(); // Read temperature as Fahrenheit
        sprintf(tdata,"T : %.1f*C",t);
        sprintf(hdata,"H : %.1f*",h);
        Serial.println(tdata);
        Serial.println(hdata);
    }
}
void setup() {
    Serial.begin(115200);

    pinMode(RELAY, OUTPUT);
    Wire.begin();
    lcd.begin(16,2);
    lcd.setBacklight(255);
    lcd.home();
    lcd.clear();
    lcd.noBlink();
    lcd.noCursor();
  
    dht.setup(14,DHTesp::DHT22);

    pinMode(REDPIN,OUTPUT);
    pinMode(BLUEPIN,OUTPUT);
    pinMode(GREENPIN,OUTPUT);
    pinMode(RELAY, OUTPUT);
    
    digitalWrite(REDPIN,LOW);
    digitalWrite(BLUEPIN,LOW);   
    digitalWrite(GREENPIN,LOW);
    digitalWrite(RELAY,LOW);    

    initDevice();
    // If not configured it'll be configured and rebooted in the initDevice(),
    // If configured, initDevice will set the proper setting to cfg variable

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    // main setup
    Serial.printf("\nIP address : "); Serial.println(WiFi.localIP());
// USER CODE EXAMPLE : meta data to local variable
    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? atoi((const char*)meta["pubInterval"]) : 0;
    lastPublishMillis = - pubInterval;
// USER CODE EXAMPLE
    
    set_iot_server();
    client.setCallback(message);
    iot_connect();
}

void loop() {
    unsigned int pump_mil = millis();
    if (!client.connected()) {
        iot_connect();
    }

    filter();
    if(f_val < 500){
      digitalWrite(REDPIN,HIGH);
      digitalWrite(BLUEPIN,HIGH);   
      digitalWrite(GREENPIN,HIGH);
    }
    else{
      digitalWrite(REDPIN,LOW);
      digitalWrite(BLUEPIN,LOW);   
      digitalWrite(GREENPIN,LOW);
    }
    GetTemperature();

    lcd.clear();
    lcd.home();
    lcd.setCursor(0,0);
    lcd.print(tdata);
    lcd.setCursor(0,1);
    lcd.print(hdata);
         
    client.loop();
    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }
}