#include <Arduino.h>
#include "IoTicosSplitter.h" //libreria para separar strings//lo usaremos para separar topicos MQTT
#include <ArduinoJson.h> //manejo de JSON y serializacion para envio y recepcion de datos arduinojson.org
#include <PubSubClient.h> //manejo de MQTT
#include <HTTPClient.h> //incluida por defecto en el core de la placa
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager librería para config. de la red, evitamos hardcoding de credenciales SSID y PASS
#include <WiFi.h>

// put function declarations here:
void WIFI_ini();
void check_mqttConn();
bool reconnect();
const char* mqtt_server = "192.168.1.20";
const char * broker_user = "dev";
const char * broker_pass = "";

WiFiClient espclient; //instancia del cliente de red
PubSubClient client(espclient); //declaracion del cliente MQTT e indicamos la conexion
long lastReconnAtt = 0;


JsonDocument mqtt_data_doc; //JsonDocument palabra reservada por la libreria ArduinoJson// reserva espacio en memoria

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
  check_mqttConn();

}

// put function definitions here:

bool reconnect(){
  //Set Mqtt sv
  client.setServer(mqtt_server, 1883);
  Serial.print("\n\n\nIntentando conexion Mqtt");

  
  //const char *str_client_id = client_id.c_str();
  String topic = "683a1f9413ca368f47a7b655/687eac95e37a35affe53f0ea/+/actdata";
  String client_id = String("device_") + random(1, 9999);
  
  if(client.connect(client_id.c_str(), broker_user, broker_pass)){
    Serial.print("\n\n\nCliente Mqtt conectado");
    delay(2000); //delay para poder ver el mensaje en monitor serial, necesario?
    client.subscribe(topic.c_str()); //el metodo espera un char array

    return true;
  }else{
    Serial.print("\n\n\nFallo conexion Mqtt");
    return false;
  }

}

void WIFI_ini(){
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.

    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
   // wm.resetSettings();

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

void check_mqttConn(){
  if(!client.connected()){
    long now = millis(); // tiempo en ms desde que inicio la placa
    if(now - lastReconnAtt > 5000){ //logica para evitar codigo bloqueante con los delay()
      lastReconnAtt = millis();
      if(reconnect()){
        lastReconnAtt = 0;
      }

    }

  }else{
    client.loop(); //metodo para procesar los mensajes mqtt llegados al buffer
  }
}