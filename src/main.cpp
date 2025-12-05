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
void checkBTN();

//VARS and library instances
String dId = "7234"; //harcoding
String _id = "687eac95e37a35affe53f0ea"; //hardcoding
String webhook_endpoint = "http://192.168.1.100:3001/api/getdevicecredentials"; //endpoint para solicitar la config del dispositivo
const char* mqtt_server = "192.168.1.100";
const char * broker_user = "dev";
const char * broker_pass = "";
WiFiClient espclient; //instancia del cliente de red
PubSubClient client(espclient); //declaracion del cliente MQTT e indicamos la conexion
IoTicosSplitter splitter; // instancia de esta libreria, usada en process_incoming_message para dividir el topico que llega
long lastReconnAtt = 0;
JsonDocument mqtt_data_doc; //JsonDocument palabra reservada por la libreria ArduinoJson// reserva espacio en memoria
WiFiManager wm;

//////DECLARACIONES USADAS PARA LA FUNCION pressBTN///////////
// Estado anterior y actual
bool estadoBotonActual = HIGH;
bool estadoBotonAnterior = HIGH;

// Tiempo de espera para 5 segundos (en milisegundos)
const unsigned long tiempoRequerido = 5000;

// Marca si ya ejecutaste el código tras 5 segundos
bool ejecutado = false;

// Tiempo cuando se detectó la presión inicial
unsigned long tPresionadoInicio = 0;

/////////////////

//PINS
#define botonPin 2
int timeout = 240;
//const int availablePins[] = { 4, 5, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 }; //14
const int availablePins[] = { 4, 16, 17, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 }; //14
const int NUM_AVAILABLE_PINS = sizeof(availablePins) / sizeof(availablePins[0]);

int leds [NUM_AVAILABLE_PINS];

void setup() {
    // put your setup code here, to run once:
    Serial.begin(115200);
    pinMode(botonPin, INPUT_PULLUP);
   WIFI_ini();

    //metodo de la libreria pubsub, funciona igual que un hook. Basicamente establecemos que queremos hacer con los mensajes que llegan al buffer, leidos gracias al metodo .loop
    client.setCallback(callback);
    //
}

void loop() {
  // put your main code here, to run repeatedly:
  checkBTN();
  check_mqttConn();
  
}

bool startConfigPortalAP() {
  const char* ap_ssid = "AutoConfigAP";
  const char* ap_pass = "password";

  Serial.println("Iniciando portal de configuración AP temporal...");
  // startConfigPortal devuelve true si el usuario terminó la configuración y el ESP conectó
  bool started = wm.startConfigPortal(ap_ssid, ap_pass);

  if (started) {
    Serial.println("Portal de configuración cerrado. Intentando conectar con credenciales nuevas...");
  } else {
    Serial.println("No se pudo iniciar o finalizar portal de configuración.");
  }
  return started;
}



void checkBTN(){
    estadoBotonActual = digitalRead(botonPin);

  // Detectar inicio de presión
  if (estadoBotonActual == LOW && estadoBotonAnterior == HIGH) {
    // El botón acaba de empezar a pulsarse
    tPresionadoInicio = millis();
    ejecutado = false; // reseteamos bandera para este ciclo de presión
  }

  // Si está presionado, calcular cuánto tiempo lleva
  if (estadoBotonActual == LOW) {
    unsigned long duracion = millis() - tPresionadoInicio;
    if (!ejecutado && duracion >= tiempoRequerido) {
      // Aquí va el código que quieres ejecutar al alcanzar 5 segundos
        
        // Borrar credenciales guardadas por WiFiManager.
        // WiFiManager almacena la configuración en el NVS/LittleFS según la plataforma.
         wm.resetSettings(); // Borra las credenciales guardadas y el portal de configuración
         ESP.restart();
    }
  } else {
    // Si se suelta, reiniciamos estado (y permitimos volver a empezar)
    ejecutado = false;
  }

  // Guardar estado para la siguiente iteración
  estadoBotonAnterior = estadoBotonActual;
 
}

void assignPinsToObjects(int count) {
  if (count > NUM_AVAILABLE_PINS) count = NUM_AVAILABLE_PINS;
  for (int i = 0; i < count; i++) {
    pinMode(availablePins[i], OUTPUT);
   digitalWrite(availablePins[i], mqtt_data_doc["actuators"][i]["value"] ? LOW : HIGH);
  }
}

// put function definitions here:
String last_rec_msg = "";
String last_rec_topic = "";
int nroPin;


void processIncomingMsg(String topic, String incoming){

  String uId = splitter.split(topic, '/', 0);
  String dId = splitter.split(topic, '/', 1);
  String hab = splitter.split(topic, '/', 2);

  last_rec_topic = uId + "/" + dId + "/" + hab + "/sdata";

  for(int i = 0; i < mqtt_data_doc["actuators"].size(); i++){
    if(mqtt_data_doc["actuators"][i]["id"] == hab){
      nroPin = i;
    }
  }

  deserializeJson(mqtt_data_doc["last"], incoming);
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
  digitalWrite(availablePins[nroPin], mqtt_data_doc["last"]["value"] ? LOW : HIGH);
}

//--> vea que el tipo de dato son punteros, es decir, el metodo setcallback nos da el espacio de direccion en memoria en el caso de topic y payload
//payload es un array de bytes, que cada uno representa un codigo ASCII
void callback(char* topic, byte* payload, unsigned int lenght){
  String incoming = "";

  for (int i = 0; i < lenght; i++){
    incoming += (char)payload[i]; //pasamos cada byte, que puntualmente es un codigo ASCII a char
  };

  incoming.trim(); //metodo nativo de String, recorta principio y final. Para evitar posibles incoherencias o saltos de linea

  processIncomingMsg(String(topic), incoming);

}

 bool res;
void WIFI_ini(){
  WiFi.mode(WIFI_STA); // modo default explícito

  // Intento inicial de conexión/configuración
  bool connected = wm.autoConnect("AutoConnectAP","password"); 

  if(connected){
    Serial.println("Conectado a WiFi mediante AutoConnect");
    return;
  } else {
    Serial.println("AutoConnect falló. Intentando portal de configuración AP temporal...");

    // Intentar AP temporal para reconfigurar credenciales
    if (startConfigPortalAP()) {
      // Después de que el usuario configure y cierre el portal, WM debería haber establecido
      // nuevas credenciales y el ESP podrá intentar conectarse nuevamente.
      // Espera breve para estabilizar la conexión
      delay(1000);

      // Intento rápido de reconectar con las nuevas credenciales proporcionadas a través del portal
      // Nota: si el portal terminó exitosamente, wm.autoConnect suele funcionar sin necesidad de reiniciar.
      if (wm.autoConnect("AutoConnectAP","password")) {
        Serial.println("Conectado tras configuración en AP temporal.");
        return;
      } else {
        Serial.println("Sin credenciales válidas tras AP temporal. Reiniciando...");
        ESP.restart();
      }
    } else {
      Serial.println("No se pudo iniciar AP temporal. Reiniciando...");
      ESP.restart();
    }
  }
}

unsigned long lastWiFiAttempt = 0;
void check_mqttConn(){
// 1) Verificar conectividad WiFi antes de cualquier operación de red
//el proceso de reconexion lo maneja la propia libreria WiFi
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
   if (now - lastWiFiAttempt >= 10000) {
      Serial.println("\nWiFi caido. Intento de reconexión...");
      lastWiFiAttempt = now;
    }
    return;
  }

  if(!client.connected()){
    long now = millis(); // tiempo en ms desde que inicio la placa
    if(now - lastReconnAtt > 5000){ //logica para evitar codigo bloqueante con los delay()
      Serial.println("\nMQTT conn caida. Intento de reconexión...");
      lastReconnAtt = millis();
      if(reconnect()){
        lastReconnAtt = 0;
      }

    }

  }else{
    client.loop(); //metodo observador, ve si hay mensajes en el buffer
  }
}