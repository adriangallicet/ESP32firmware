#include <Arduino.h>
#include "IoTicosSplitter.h" //libreria para separar strings//lo usaremos para separar topicos MQTT
#include <ArduinoJson.h> //manejo de JSON y serializacion para envio y recepcion de datos arduinojson.org
#include <PubSubClient.h> //manejo de MQTT
#include <HTTPClient.h> //incluida por defecto en el core de la placa
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager librería para config. de la red, evitamos hardcoding de credenciales SSID y PASS
#include <WiFi.h>

// declaracion de funciones
void WIFI_ini();
void check_Conectividad();
bool reconnect();
void callback(char* topic, byte* payload, unsigned int lenght); //el manejo de datos, es decir, que sean punteros y el tipo en cada caso, son disposiciones de la propia libreria PubSubClient
void processIncomingMsg(String topic, String incoming);
void send_data_broker();
bool getDeviceConfig();
void assignPinsToObjects(int count);
void checkBTN();
bool startConfigPortalAP();

//instancias de variables y librerias
String dId = "7234"; //harcoding
String _id = "687eac95e37a35affe53f0ea"; //hardcoding
String webhook_endpoint = "http://192.168.1.100:3001/api/getdevicecredentials"; //endpoint para solicitar la config del dispositivo
const char* mqtt_server = "192.168.1.100";
const char * broker_user = "dev";//usuario y contrasenia para conectarse al broker. En esta etapa de desarrollo se utilizan genericas
const char * broker_pass = "";
WiFiClient espclient; //instancia del cliente de red
PubSubClient client(espclient); //declaracion del cliente MQTT e indicamos la conexion
IoTicosSplitter splitter; // instancia de esta libreria, usada en process_incoming_message para dividir el topico que llega
long lastReconnAtt = 0;
JsonDocument mqtt_data_doc; //JsonDocument palabra reservada por la libreria ArduinoJson// reserva espacio en memoria
WiFiManager wm;
unsigned long lastWiFiAttempt = 0;
int nroPin;
String last_rec_topic = "";

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
#define botonPin 32
int timeout = 240;
const int availablePins[] = { 4, 16, 17, 18, 19, 21, 22, 23, 13, 14, 27, 25, 33 }; //13
const int NUM_AVAILABLE_PINS = sizeof(availablePins) / sizeof(availablePins[0]);

int leds [NUM_AVAILABLE_PINS];

void setup() {
    // put your setup code here, to run once:
    Serial.begin(115200);
    pinMode(botonPin, INPUT_PULLUP);
    //metodo de la libreria pubsub, funciona igual que un hook. Basicamente establecemos que queremos hacer con los mensajes MQTT que llegan al bufferleidos gracias al metodo .loop
    client.setCallback(callback);
    WIFI_ini();
}

void loop() {

  checkBTN();
  check_Conectividad();
   
}

//setup
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

//0)Deteccion de flanco de bajada usando pull-up interno y, si se mantiene pulsado durante 5s. Borra credenciales Wifi y reinicia placa.
//funcion pensada para utilizar si se quiere usar el dispositivo en otra red, o si cambia SSID y pass de la actual
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
      // Borrar credenciales guardadas por WiFiManager.
         wm.resetSettings();
      // Reset de placa
         ESP.restart();
    }
  } else {
    // Si se suelta, reiniciamos estado (y permitimos volver a empezar)
    ejecutado = false;
  }

  // Guardar estado para la siguiente iteración
  estadoBotonAnterior = estadoBotonActual;
 
}

//1) funcion principal se verifica conexion WIFI, MQTT y si todo esta conectado, lee el buffer de mensajes MQTT
void check_Conectividad(){
// 1) Verificar conectividad WiFi antes de cualquier operación
//el proceso de reconexion lo maneja la propia libreria WiFi
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
   if (now - lastWiFiAttempt >= 10000) {
      Serial.println("\nWiFi caido. Intento de reconexion...");
      lastWiFiAttempt = now;
    }
    return;
  }

// 2) Verificar conectividad MQTT
//si el cliente no esta conectado, intenta conexion mediante reconnect()
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
//1.a)funcion utilizada en check_Conectividad() se encarga de la conexion y subscripcion MQTT
bool reconnect(){

  //El dispositivo recibe su configuracion guardada en BD mediante solitud HTTP a la API en base a getDeviceConfig()
  ///mediante este condicional, se verifica en memoria volatil si se ha recibido la config. Evitamos otra solicitud HTTP redundante
  if (!mqtt_data_doc.isNull()) {
      Serial.println("\nya se recibio la config. no es necesario volver a pedirla");
    }
  else if(!getDeviceConfig()){
    return false;
  }
  ///
  Serial.println("\nIntentando conexion MQTT");

  //Set Mqtt sv
  client.setServer(mqtt_server, 1883);

 // String topic = "683a1f9413ca368f47a7b655/687eac95e37a35affe53f0ea/+/actdata";
  String client_id = String("device_") + dId + "_" + random(1, 9999);
  
  String str_topic = mqtt_data_doc["topic"];
  
  //Vea que los metodos connect y subscribe esperan array de caracteres, para eso se utiliza el metodo .c_str() nativo de c++
  if(client.connect(client_id.c_str(), broker_user, broker_pass)){
    Serial.print("\nCliente Mqtt conectado");
    client.subscribe((str_topic + "+/actdata").c_str()); //al lograr conexion, intentamos sub al topico
    //el back responde algo de la forma: id.user/_id del dispositivo/ ---> 683a1f9413ca368f47a7b655/687eac95e37a35affe53f0ea/ por eso agregamos /actdata"                                    
    return true;
  }else{
    Serial.print("\nFallo conexion Mqtt");
    return false;
  }

}
//1.b)para que el dispositivo pueda conectarse a mqtt mediante la funcion reconnect() necesita obtener su configuracion desde BD, lo realiza mediante getDeviceConfig()
bool getDeviceConfig(){
  Serial.print("\nObteniendo configuraciones del dispositivo desde la plataforma");

  String toSend = "dId=" + dId + "&_id=" + _id;

  HTTPClient http; // declaracion de instancia de la libreria
  http.begin(webhook_endpoint); //metodo para iniciar cliente
  http.addHeader("Content-Type", "application/x-www-form-urlencoded"); //headers

  int res_code = http.POST(toSend); //este metodo genera la peticion devuelve el codigo de respuesta HTTP de ella (es decir, 200, 401, etc.)

  if(res_code < 0 ){
    Serial.print("\nError enviando la peticion HTTP POST");
    http.end(); //cerramos comunicacion
    return false;
  }
  if(res_code != 200){
    Serial.print("\nError en la respuesta" + res_code);
    http.end(); //cerramos comunicacion
    return false;
  }
  if(res_code == 200){
    String resBody = http.getString(); //este metodo devuelve el body de la respuesta en forma de String
    Serial.print("\ndev config obtenidas exitosamente!!!!!!!!!");
    deserializeJson(mqtt_data_doc, resBody);
    assignPinsToObjects(mqtt_data_doc["actuators"].size());
    http.end();
    return true;
  }

  return false;
}
//1.b).1 esta funcion recibe como entrada el numero de actuadores a partir de getDeviceConfig()
//en base a ello configura como salida los pines y da los valores correspondientes que recibe desde BD
void assignPinsToObjects(int count) {
  if (count > NUM_AVAILABLE_PINS) count = NUM_AVAILABLE_PINS;
  for (int i = 0; i < count; i++) {
    pinMode(availablePins[i], OUTPUT);
   digitalWrite(availablePins[i], mqtt_data_doc["actuators"][i]["value"] ? LOW : HIGH);
  }
}

//2)esta funcion es ejecutada cada vez que se lee el buffer MQTT y hay un mensaje en el
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
//2.a)
void processIncomingMsg(String topic, String incoming){

  String uId = splitter.split(topic, '/', 0); //id de usuario que envia
  String dId = splitter.split(topic, '/', 1); //id de dispositivo que envia
  String hab = splitter.split(topic, '/', 2); //numero que identifica el rele al que se desea cambiar su estado

  last_rec_topic = uId + "/" + dId + "/" + hab + "/sdata"; //ya dejo listo el topico a donde se publicara el mensaje de eco para dar ok al procedimiento. dicha variable se utiliza en send_data_broker();

  //mediante este for, identificamos el numero de indice que coincide con el actuador
  for(int i = 0; i < mqtt_data_doc["actuators"].size(); i++){
    if(mqtt_data_doc["actuators"][i]["id"] == hab){
      nroPin = i;
    }
  }

  deserializeJson(mqtt_data_doc["last"], incoming);
  serializeJsonPretty(mqtt_data_doc, Serial);
  //estado alto o bajo en la salida digital del rele correspondiente, dependiendo del mensaje recibido 
  digitalWrite(availablePins[nroPin], mqtt_data_doc["last"]["value"] ? LOW : HIGH);
  send_data_broker();
}
//2.a).1 se envia al broker el mismo mensaje que se ha recibido a modo de eco. Indicando que el proceso de cambiar el estado del actuador fue correcto
void send_data_broker(){
  String toSend = "";
  serializeJson(mqtt_data_doc["last"], toSend);
  client.publish(last_rec_topic.c_str(), toSend.c_str());
  //se limpia en la memoria
  mqtt_data_doc["last"]["value"] = "";
} 