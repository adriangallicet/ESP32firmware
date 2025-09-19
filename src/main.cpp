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
void callback(char* topic, byte* payload, unsigned int lenght); //el manejo de datos, es decir, que sean punteros y el tipo en cada caso, son disposiciones de la propia libreria PubSubClient
void processIncomingMsg(String topic, String incoming);
void process_actuators();
void send_data_broker();

//VARS and library instances
const char* mqtt_server = "192.168.1.20";
const char * broker_user = "dev";
const char * broker_pass = "";
WiFiClient espclient; //instancia del cliente de red
PubSubClient client(espclient); //declaracion del cliente MQTT e indicamos la conexion
IoTicosSplitter splitter; // instancia de esta libreria, usada en process_incoming_message para dividir el topico que llega
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
    client.setCallback(callback); //metodo de la libreria pubsub, funciona igual que un hook. Basicamente establecemos que queremos hacer con los mensajes que llegan al buffer, leidos gracias al metodo .loop
}

void loop() {
  // put your main code here, to run repeatedly:
  check_mqttConn();

}

// put function definitions here:
String last_rec_msg = "";
String last_rec_topic = "";


void processIncomingMsg(String topic, String incoming){
 // last_rec_topic = topic;
  //last_rec_msg = incoming;
  String uId = splitter.split(topic, '/', 0);
  String dId = splitter.split(topic, '/', 1);
  String hab = splitter.split(topic, '/', 2);

  last_rec_topic = uId + "/" + dId + "/" + hab + "/sdata";

  JsonDocument doc;
  //transformamos JSON a un doc que maneja la libreria ArduinoJson. Dicho doc es lo que para Javascrypt un objeto.
  deserializeJson(mqtt_data_doc["last"], incoming);
  //mqtt_data_doc["last"] = doc;

  serializeJsonPretty(mqtt_data_doc, Serial);
  process_actuators();
  send_data_broker();

}

void send_data_broker(){
  String toSend = "";
  serializeJson(mqtt_data_doc["last"], toSend);
  client.publish(last_rec_topic.c_str(), toSend.c_str());
  mqtt_data_doc["last"]["value"] = "";
}

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
void process_actuators(){
  if (mqtt_data_doc["last"]["value"] == true){
    digitalWrite(led, HIGH);
  }
  else if (mqtt_data_doc["last"]["value"] == false)
  {
    digitalWrite(led, LOW);
  }
  
}

void callback(char* topic, byte* payload, unsigned int lenght){
  String incoming = "";

  for (int i = 0; i < lenght; i++){
    incoming += (char)payload[i]; //pasamos cada byte, que puntualmente es un codigo ASCII a char
  };

  incoming.trim(); //metodo de String, recorta principio y final. Para evitar posibles incoherencias o saltos de linea

  processIncomingMsg(String(topic), incoming);

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
    client.loop(); //metodo observador, ve si hay mensajes en el buffer
  }
}