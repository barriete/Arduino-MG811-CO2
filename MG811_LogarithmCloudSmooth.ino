#include <DHT.h>
#include <ThingSpeak.h>
#include <WiFiNINA.h>
#include <SoftTimers.h>
#include "arduino_secrets.h"

char ssid[] = SECRET_SSID;   // your network SSID (name)
char pass[] = SECRET_PASS;   // your network password
WiFiClient  client;

unsigned long myChannelNumber = xxxxxxx;
const char * myWriteAPIKey = "xxxxxxxxx";

const int sensorpsicrometrico = 5; //Pin Sensor DHT22
DHT dht5(sensorpsicrometrico, DHT22);

const int numLecturas = 5;    //numero de lecturas para hacer la media
int indiceLectura = 0;              // indice de lectura actual

SoftTimer cadenciaLecturas;
SoftTimer cadenciaCloud;

float humedad[numLecturas];      // matriz con las lecturas
float humedadAcumulada = 0;                  // acumulado actual
float humedadMedia = 0;

float temperatura[numLecturas];      // matriz con las lecturas
float temperaturaAcumulada = 0;                  // acumulado actual
float temperaturaMedia = 0;

int porcentajeCO2[numLecturas];
int porcentajeCO2Acumulada = 0;
int porcentajeCO2Media = 0;

float voltaje[numLecturas];      // matriz con las lecturas
float voltajeAcumulada = 0;                  // acumulado actual
float voltajeMedia = 0;

int number = 0;
int percentage;
float volts;

/************************Hardware Related Macros************************************/
#define         MG_PIN                       (0)     //define which analog input channel you are going to use
#define         BOOL_PIN                     (2)
#define         DC_GAIN                      (8.5)   //define the DC gain of amplifier

/***********************Software Related Macros************************************/
#define         READ_SAMPLE_INTERVAL         (100)    //define how many samples you are going to take in normal operation
#define         READ_SAMPLE_TIMES            (10)     //define the time interval(in milisecond) between each samples in normal operation

/**********************Application Related Macros*********************************

*/
//These two values differ from sensor to sensor. user should derermine this value.
#define         ZERO_POINT_VOLTAGE           (0.429) //define the output of the sensor in volts when the concentration of CO2 is 400PPM
#define         REACTION_VOLTAGE             (0.080) //define the voltage drop of the sensor when move the sensor from air into 1000ppm CO2

/*****************************Globals***********************************************/
float           CO2Curve[3]  =  {2.602, ZERO_POINT_VOLTAGE, (REACTION_VOLTAGE / (2.602 - 3))};
//two points are taken from the curve.
//with these two points, a line is formed which is
//"approximately equivalent" to the original curve.
//data format:{ x, y, slope}; point1: (lg400, 0.324), point2: (lg1000, 0.280)
//slope = ( reaction voltage ) / (log400 –log1000)

float minvolts = ZERO_POINT_VOLTAGE;
float maxvolts = ZERO_POINT_VOLTAGE;

void setup() {
  Serial.begin(9600);                              //UART setup, baudrate = 9600bps
  pinMode(BOOL_PIN, INPUT);                        //set pin to input
  digitalWrite(BOOL_PIN, HIGH);                    //turn on pullup resistors
  dht5.begin();
  ThingSpeak.begin(client);  // Initialize ThingSpeak
  cadenciaLecturas.setTimeOutTime(60000); // every 60 seconds.
  cadenciaLecturas.reset();

  //Crear las matrices
  //Matriz Temperatura
  for (int thisReading = 0; thisReading < numLecturas; thisReading++) {
    temperatura[numLecturas] = 0;
  }
  //Matriz Humedad
  for (int thisReading = 0; thisReading < numLecturas; thisReading++) {
    humedad[numLecturas] = 0;
  }
  //Matriz CO2
  for (int thisReading = 0; thisReading < numLecturas; thisReading++) {
    porcentajeCO2[numLecturas] = 0;
  }
  //Matriz CO2
  for (int thisReading = 0; thisReading < numLecturas; thisReading++) {
    voltaje[numLecturas] = 0;
  }
}

void loop() {
  // Connect or reconnect to WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    while (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, pass); // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(".");
      delay(5000);
    }
    Serial.println("\nConnected");
  }

  volts = MGRead(MG_PIN);
  // This lines are only for analysis purpose
  //Serial.print("MinVolts:");
  //Serial.print(minvolts, 3);
  //Serial.print("  MaxVolts:");
  //Serial.println(maxvolts, 3);
  Serial.print( "Current Voltage:" );
  Serial.print(volts, 3);
  Serial.print( "V" );

  // Applying correction based on humidity
  volts = volts + 0.059 * log(dht5.readHumidity())- 0.234;
  Serial.print( "Corrected Voltage:" );
  Serial.print(volts, 3);
  Serial.print( "V" );
  
  if (minvolts > volts) {
    minvolts = volts;
  }
  else

    if (maxvolts < volts) {
      maxvolts = volts;
    }

  percentage = MGGetPercentage(volts, CO2Curve);
  Serial.print(" CO2:");
  if (percentage == -1) {
    Serial.print( "<400" );
  } else {
    Serial.print(percentage);
  }
  Serial.println( "ppm" );

  if (cadenciaLecturas.hasTimedOut())
  {
    lecturas();
    cadenciaLecturas.reset(); //next refresh in 30 seconds
    Serial.print("Proximo promedio en: ");
    Serial.print(60000);
    Serial.println(" segs");
  }
}

/*****************************  MGRead *********************************************
  Input:   mg_pin - analog channel
  Output:  output of SEN-000007
  Remarks: This function reads the output of SEN-000007
  MKR Resolucion for Analogread is 3,3 volts and 4096
************************************************************************************/
float MGRead(int mg_pin)
{
  int i;
  float v = 0;
  float v2 = 0;

  for (i = 0; i < READ_SAMPLE_TIMES; i++) {
    v += analogRead(mg_pin);
    delay(READ_SAMPLE_INTERVAL);
  }
  //v2 = (v / READ_SAMPLE_TIMES);
  //Serial.println(v2);
  v = (v / READ_SAMPLE_TIMES) * 3.3 / 4096 ;
  //Serial.println(v, 3);

  return v;
}

/*****************************  MQGetPercentage **********************************
  Input:   volts   - SEN-000007 output measured in volts
         pcurve  - pointer to the curve of the target gas
  Output:  ppm of the target gas
  Remarks: By using the slope and a point of the line. The x(logarithmic value of ppm)
         of the line could be derived if y(MG-811 output) is provided. As it is a
         logarithmic coordinate, power of 10 is used to convert the result to non-logarithmic
         value.
************************************************************************************/
int  MGGetPercentage(float volts, float * pcurve)
{
  if ((volts / DC_GAIN ) >= ZERO_POINT_VOLTAGE) {
    return -1;
  } else {
    return pow(10, ((volts) - pcurve[1]) / pcurve[2] + pcurve[0]);
  }
}

void lecturas() {
  int toCloud = 0;

  temperatura[indiceLectura] = dht5.readTemperature();      //Leemos la temperatura interior
  temperaturaAcumulada = temperaturaAcumulada + temperatura[indiceLectura];

  humedad[indiceLectura] = dht5.readHumidity();      //Leemos la Humedad interior
  humedadAcumulada = humedadAcumulada + humedad[indiceLectura];

  porcentajeCO2[indiceLectura] = percentage;      //Leemos la temperatura interior
  porcentajeCO2Acumulada = porcentajeCO2Acumulada + porcentajeCO2[indiceLectura];

  voltaje[indiceLectura] = volts;      //Leemos el voltaje
  voltajeAcumulada = voltajeAcumulada + voltaje[indiceLectura];

  temperaturaMedia = temperaturaAcumulada / numLecturas;
  humedadMedia = humedadAcumulada / numLecturas;
  porcentajeCO2Media = porcentajeCO2Acumulada / numLecturas;
  voltajeMedia = voltajeAcumulada / numLecturas;
  
  indiceLectura = indiceLectura + 1;

  if (indiceLectura >= numLecturas) {
    indiceLectura = 0;
    toCloud = 1;
  }

  temperaturaAcumulada = temperaturaAcumulada - temperatura[indiceLectura];          //Resta del Total el último valor
  humedadAcumulada = humedadAcumulada - humedad[indiceLectura];          //Resta del Total el último valor
  porcentajeCO2Acumulada = porcentajeCO2Acumulada - porcentajeCO2[indiceLectura];          //Resta del Total el último valor
  voltajeAcumulada = voltajeAcumulada - voltaje[indiceLectura];          //Resta del Total el último valor

  Serial.println("Magnit.|Valor |Mat(0)|Mat(1)|Mat(2)|Mat(3)|Mat(4)|Acumul.|Media |");
  Serial.print("Temp.  | ");
  Serial.print(dht5.readTemperature());
  Serial.print("| ");
  Serial.print(temperatura[0]);
  Serial.print("| ");
  Serial.print(temperatura[1]);
  Serial.print("| ");
  Serial.print(temperatura[2]);
  Serial.print("| ");
  Serial.print(temperatura[3]);
  Serial.print("| ");
  Serial.print(temperatura[4]);
  Serial.print("|  ");
  Serial.print(temperaturaAcumulada);
  Serial.print("|");
  Serial.print(temperaturaMedia);
  Serial.println(" |");
  Serial.print("Humedad| ");
  Serial.print(dht5.readHumidity());
  Serial.print("| ");
  Serial.print(humedad[0]);
  Serial.print("| ");
  Serial.print(humedad[1]);
  Serial.print("| ");
  Serial.print(humedad[2]);
  Serial.print("| ");
  Serial.print(humedad[3]);
  Serial.print("| ");
  Serial.print(humedad[4]);
  Serial.print("| ");
  Serial.print(humedadAcumulada);
  Serial.print("| ");
  Serial.print(humedadMedia);
  Serial.println("|");
  Serial.print("CO2 ppm|");
  Serial.print(percentage);
  Serial.print("   |");
  Serial.print(porcentajeCO2[0]);
  Serial.print("   |");
  Serial.print(porcentajeCO2[1]);
  Serial.print("   |");
  Serial.print(porcentajeCO2[2]);
  Serial.print("   |");
  Serial.print(porcentajeCO2[3]);
  Serial.print("   |");
  Serial.print(porcentajeCO2[4]);
  Serial.print("   |");
  Serial.print(porcentajeCO2Acumulada);
  Serial.print("   |");
  Serial.print(porcentajeCO2Media);
  Serial.println("   |");
  Serial.print("voltaje|");
  Serial.print(volts,3);
  Serial.print(" |");
  Serial.print(voltaje[0],3);
  Serial.print(" |");
  Serial.print(voltaje[1],3);
  Serial.print(" |");
  Serial.print(voltaje[2],3);
  Serial.print(" |");
  Serial.print(voltaje[3],3);
  Serial.print(" |");
  Serial.print(voltaje[4],3);
  Serial.print(" |");
  Serial.print(voltajeAcumulada,3);
  Serial.print("  |");
  Serial.print(voltajeMedia,3);
  Serial.println(" |");

  /*
    Serial.print(dht5.readTemperature());
    Serial.print(" / ");
    Serial.println(temperaturaMedia);
    Serial.print("Humedad: ");
    Serial.print(dht5.readHumidity());
    Serial.print(" / ");
    Serial.println(humedadMedia);
    Serial.print("CO2: ");
    Serial.print(percentage);
    Serial.print(" / ");
    Serial.println(porcentajeCO2Media);
  */
  if (toCloud == 1) {
    Serial.println("Actualiza cloud ...");
    updatecloud();
    toCloud = 0;
  }
}

void updatecloud() {
  ThingSpeak.setField(1, temperaturaMedia);
  ThingSpeak.setField(2, humedadMedia);
  ThingSpeak.setField(3, porcentajeCO2Media);
  ThingSpeak.setField(4, voltajeMedia);
  //ThingSpeak.setField(3, percentage);

  // Write to ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different
  // pieces of information in a channel.  Here, we write to field 1.
  Serial.print("Temperatura: ");
  Serial.print(temperaturaMedia, 2);
  Serial.print(" ºC, ");
  Serial.print("Humedad: ");
  Serial.print(humedadMedia, 2);
  Serial.println(" %HR ");
  Serial.print(" PPM: ");
  Serial.print(porcentajeCO2Media);
  Serial.print(" Volts: ");
  Serial.println(voltajeMedia);

  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (x == 200) {
    Serial.println("Channel update successful.");
  }
  else {
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }

}
