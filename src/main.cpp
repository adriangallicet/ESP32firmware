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
bool getDeviceCredentials();
void assignPinsToObjects(int count);

//VARS and library instances
String dId = "7234"; //harcoding
String _id = "687eac95e37a35affe53f0ea"; //hardcoding
String webhook_endpoint = "http://192.168.1.20:3001/api/getdevicecredentials"; //endpoint para solicitar la config del dispositivo
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

const int availablePins[] = { 4, 5, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 }; //14
const int NUM_AVAILABLE_PINS = sizeof(availablePins) / sizeof(availablePins[0]);

struct LedControl {
  int pin;
  bool state;
};

LedControl leds[NUM_AVAILABLE_PINS];

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

void assignPinsToObjects(int count) {
  if (count > NUM_AVAILABLE_PINS) count = NUM_AVAILABLE_PINS;
  for (int i = 0; i < count; i++) {
    leds[i].pin = availablePins[i];
    leds[i].state = false;
    pinMode(leds[i].pin, OUTPUT);
    digitalWrite(leds[i].pin, LOW);
  }
  // Limpia los que no se asignan
  for (int i = count; i < NUM_AVAILABLE_PINS; i++) {
    leds[i].pin = -1;
    leds[i].state = false;
  }
}

// put function definitions here:
String last_rec_msg = "";
String last_rec_topic = "";
int nroPin;


void processIncomingMsg(String topic, String incoming){
 // last_rec_topic = topic;
  //last_rec_msg = incoming;
  String uId = splitter.split(topic, '/', 0);
  String dId = splitter.split(topic, '/', 1);
  String hab = splitter.split(topic, '/', 2);

  last_rec_topic = uId + "/" + dId + "/" + hab + "/sdata";

  for(int i = 0; i < mqtt_data_doc["actuators"].size(); i++){
    if(mqtt_data_doc["actuators"][i]["id"] == hab){
      nroPin = i;
      Serial.print("\n\nla posicion del array es");
      Serial.print(nroPin);
    }
  }

  JsonDocument doc;
  //transformamos JSON a un doc que maneja la libreria ArduinoJson. Dicho doc es lo que para Javascrypt un objeto.
  deserializeJson(mqtt_data_doc["last"], incoming);
  //mqtt_data_doc["last"] = doc;
  serializeJsonPretty(mqtt_data_doc, Serial);
  process_actuators();
  send_data_broker();

}

void send_data_broker(){

  String str_root_topic = mqtt_data_doc["topic"];
  String topic = str_root_topic + "sdata";
  String toSend = "";

  serializeJson(mqtt_data_doc["last"], toSend);
  client.publish(last_rec_topic.c_str(), toSend.c_str());
  mqtt_data_doc["last"]["value"] = "";
}

bool getDeviceCredentials(){
  Serial.print("\n\nObteniendo configuraciones del dispositivo desde la plataforma");
  delay(1000);

  String toSend = "dId=" + dId + "&_id=" + _id;

  HTTPClient http; // declaracion de instancia de la libreria
  http.begin(webhook_endpoint); //metodo para iniciar cliente
  http.addHeader("Content-Type", "application/x-www-form-urlencoded"); //headers

  int res_code = http.POST(toSend); //este metodo genera la peticion devuelve el codigo de respuesta HTTP de ella (es decir, 200, 401, etc.)

  if(res_code < 0 ){
    Serial.print("\n\nError enviando la peticion HTTP POST");
    http.end(); //cerramos comunicacion
    return false;
  }
  if(res_code != 200){
    Serial.print("\n\nError en la respuesta" + res_code);
    http.end(); //cerramos comunicacion
    return false;
  }

  if(res_code == 200){
    String resBody = http.getString(); //este metodo devuelve el body de la respuesta en forma de String
    Serial.print("\n\n\ndev config obtenidas exitosamente!!!!!!!!!");
    deserializeJson(mqtt_data_doc, resBody);
    assignPinsToObjects(mqtt_data_doc["actuators"].size());
    //Serial.print("\n\n" + resBody);
    http.end();
    delay(1000);
    return true;
  }

    return false;
}

bool reconnect(){
  //en caso de que haya algun tipo de error obteniendo la config. del device desde backend, espera de 10sec y restart la placa
  if(!getDeviceCredentials()){
    Serial.println("RESTART EN 10 SEC");
    delay(10000);
    ESP.restart();
  }
  //

  //Set Mqtt sv
  client.setServer(mqtt_server, 1883);
  Serial.print("\n\n\nIntentando conexion Mqtt");


 // String topic = "683a1f9413ca368f47a7b655/687eac95e37a35affe53f0ea/+/actdata";
  String client_id = String("device_") + dId + "_" + random(1, 9999);
  
  String str_topic = mqtt_data_doc["topic"];
  
  //Vea que los metodos connect y subscribe esperan array de caracteres, para eso se utiliza el metodo .c_str() nativo de c++
  if(client.connect(client_id.c_str(), broker_user, broker_pass)){
    Serial.print("\n\n\nCliente Mqtt conectado");
    delay(2000); //delay para poder ver el mensaje en monitor serial, necesario?
    client.subscribe((str_topic + "+/actdata").c_str()); //al lograr conexion, intentamos sub al topico
    //el back responde algo de la forma: id.user/_id del dispositivo/ ---> 683a1f9413ca368f47a7b655/687eac95e37a35affe53f0ea/ por eso agregamos /actdata"                                    
    return true;
  }else{
    Serial.print("\n\n\nFallo conexion Mqtt");
    return false;
  }

}
void process_actuators(){
  if (mqtt_data_doc["last"]["value"] == true){
    digitalWrite(leds[nroPin].pin, HIGH);
  }
  else if (mqtt_data_doc["last"]["value"] == false)
  {
    digitalWrite(leds[nroPin].pin, LOW);
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

 bool res;
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
//TESTEAR SI FUNCIONA, VER QUE PASA SI SE CAE LA CON WIFI.
  if(!res){
    Serial.print("\n\n\nConexion Wifi fallo, reinicio de dispositivo...");
    delay(15000);
    ESP.restart();
  };
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