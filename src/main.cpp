#include <Arduino.h>
#include "IoTicosSplitter.h" //libreria para separar strings//lo usaremos para separar topicos MQTT
#include <ArduinoJson.h> //manejo de JSON
#include <PubSubClient.h> //manejo de MQTT
#include <HTTPClient.h> //incluida por defecto en el core de la placa
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager librería para config. de la red, evitamos hardcoding de credenciales SSID y PASS

// put function declarations here:
void WIFI_ini();
const char* mqtt_server = "192.168.1.20";

//PINS
#define led 2

//


void setup() {

    // put your setup code here, to run once:
    Serial.begin(115200);
    pinMode(led, OUTPUT);

    WIFI_ini();
    

}

void loop() {
  // put your main code here, to run repeatedly:

}

// put function definitions here:

void WIFI_ini(){
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.

    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    res = wm.autoConnect("AutoConnectAP","password"); // password protected ap

    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    }
}
