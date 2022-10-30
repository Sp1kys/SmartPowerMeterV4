#ifdef ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #include <Spiffs.h>
  #include <HTTPClient.h>
  #include <FS.h>
  #include <PubSubClient.h>
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  #include <espmdns.h>
#endif

#ifdef ESP8266
  #include <ESPAsyncTCP.h>
  #include <ESP8266HTTPClient.h>
  #include <ESP8266WiFi.h>
  #include <Hash.h>
  #include <FS.h>
  #include <LittleFS.h>
#endif

#include <ADE9153A.h>
#include <ADE9153AAPI.h>
#include <Arduino.h>
#include <Wire.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include <SPI.h>
#include "FFT.h" // include the library
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h> 


#define OTA_PASSDW "admin"
#define DEBUG true // false: OTA_PASSDW enabled; true: OTA_PASSDW disabled

#define DEBUG_DUT false

//#define OLD_BOARD
#define NEW_BOARD

#define MAX_LENGTH_TELNET_PW 50
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels


AsyncWebServer server2(80);
DNSServer dns;

// Create an Event Source on /events
AsyncEventSource events("/events");

WiFiClient wifiClient;

//String apiKey = "cc9a44ab7f1a994adfc7a7a21f29529d";
String apiKey = "1acc2f443e9c0c49a8211d33e14ac873";

/* Basic initializations */
#define SPI_SPEED 10000000     //SPI Speed

/*Esp32 pins CS 5, Mosi 23, Miso 19, Sclk 18; ESP32c3 CS 7, Mosi 6, Miso 5, Sclk 4*/

#ifdef ESP32

  #define CS_PIN 5            //5-esp32 devkit, 7-esp32c3 devkit mini, 
  
  #ifdef OLD_BOARD
  
    #define ADE9153A_RESET_PIN 19 //19-esp32 devkit, 5-esp32c3 devkit mini, 17 for v4 board}

  #endif

  #ifdef NEW_BOARD

    #define ADE9153A_RESET_PIN 17 //19-esp32 devkit, 5-esp32c3 devkit mini, 17 for v4 board

  #endif
  
#endif

#ifdef ESP8266
  #define CS_PIN 15              //8-->Arduino Zero. 15-->ESP8266 
  #define ADE9153A_RESET_PIN 12 //On-board Reset Pin
#endif

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

ADE9153AClass ade9153A;

struct EnergyRegs energyVals;  //Energy register values are read and stored in EnergyRegs structure
struct PowerRegs powerVals;    //Metrology data can be accessed from these structures
struct RMSRegs rmsVals;  
struct PQRegs pqVals;
struct AcalRegs acalVals;
struct Temperature tempVal;
struct HalfRMSRegs halfrmsVals;


void readandwrite(void);
void resetADE9153A(void);
void runLength(unsigned long seconds);
void measureWaveForm(int cycles);
double doFFT(int cycles, int select);
uint32_t SPI_Read_32(uint16_t Address);
void configModeCallback (AsyncWiFiManager *myWiFiManager);
void wifiManagerInit();
void OTAinit();
void calibrate();
void initWebServer();
int trigger(int cycles, float triggerLevel, unsigned long timeout, int select);
String processor(const String& var);
String outputState(int output);
void readEnergyValues(void);
void callback(char* topic, byte* payload, unsigned int length);
void displayInit();
void updateDisplay();

void advTask( void * parameter );
void saveEnergyTask( void * parameter );
void triggerTask( void * parameter );
void uptimeTask( void * parameter );
void sendDataToDBTask( void * parameter );
void OTATask( void * parameter );
void displayUpdateTask( void * parameter );
void calibTask( void * parameter );

const int blinkInterval = 5000;
const int kordus2 = 1024;
double THDV = 0;
double THDI = 0;
float realEnergy = 0;
float reactiveEnergy = 0;
float apparentEnergy = 0;
int relay = 1;
const float samplingFrequency = 4000;//3415, 2048
String triggerData;
String triggerData2;
String Message1;
String Message2;
String Message3;
String Message4;
int measMode = 0;
const char* PARAM_INPUT_1 = "output";
const char* PARAM_INPUT_2 = "state";

const char* PARAM_INPUT_TRIGGER_COUNT = "count";
const char* PARAM_INPUT_TRIGGER_LEVEL = "level";
const char* PARAM_INPUT_TRIGGER_TIMEOUT = "timeout";
const char* PARAM_INPUT_TRIGGER_SELECT = "select";

int waveState = 0;
int relayState = 1;
int wifiWTD = 0;

#ifdef OLD_BOARD
  
    String DevID = "\"RocketSocket3\""; //Seadme nimi  

  #endif

  #ifdef NEW_BOARD

    String DevID = "\"RocketSocket1\""; //Seadme nimi  

  #endif




TaskHandle_t Task1, Task2, Task3, Task4, Task5, Task6, Task7, Task8, Task9;

void setup() {
  
  pinMode(ADE9153A_RESET_PIN, OUTPUT);
  pinMode(relay, OUTPUT);
  digitalWrite(ADE9153A_RESET_PIN, HIGH);
  digitalWrite(relay, LOW);
  

  if (DEBUG_DUT == true) Serial.begin(115200);
  

  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  else{
    Serial.println("SPIFFS mounted successfully");
  }
  Serial.println("Booting");

  displayInit();
  readEnergyValues(); 
  wifiManagerInit();
  OTAinit();
  initWebServer();
  resetADE9153A();

  delay(500);
  
  xTaskCreate(saveEnergyTask,"Energy",4096,NULL,1,&Task4);
  delay(100);
  xTaskCreate(uptimeTask,"Uptime",4096,NULL,1,&Task6);
  delay(100);
  xTaskCreate(sendDataToDBTask,"Send2DB",8192,NULL,1,&Task7);
  delay(100);
  xTaskCreate(OTATask,"OTA",4096,NULL,1,&Task8);
  delay(100);
  xTaskCreate(displayUpdateTask,"OLED",4096,NULL,1,&Task9);
  
}
  
void loop() {
  yield();
}

void readandwrite()
{ 
 /* Read and Print WATT Register using ADE9153A Read Library */
  ade9153A.ReadPowerRegs(&powerVals); 
  ade9153A.ReadRMSRegs(&rmsVals);
  ade9153A.ReadPQRegs(&pqVals);
  ade9153A.ReadTemperature(&tempVal);
  ade9153A.ReadEnergyRegs(&energyVals);
  ade9153A.ReadHalfRMSRegs(&halfrmsVals);

  if(WiFi.status()== WL_CONNECTED){
    
    // Your Domain name with URL path or IP address with path
    realEnergy = realEnergy + energyVals.ActiveEnergyValue/1000;
    reactiveEnergy = reactiveEnergy + energyVals.FundReactiveEnergyValue/1000;
    apparentEnergy = apparentEnergy + energyVals.ApparentEnergyValue/1000;

    //String serverName1 = "http://192.168.50.154:8080/input/post?node=esp32v4&fulljson=";
    String serverName1 = "http://rocket-socket.herokuapp.com/api/socket";
    //String serverName1 = "http://ptsv2.com/t/yx1ay-1666997073/post";
    String Json = "";

    if (measMode == 0)
    {
      /*
      Json = "{\"Voltage\":"+ String(rmsVals.VoltageRMSValue/1000,3) + ",\"Current\":"+ String(rmsVals.CurrentRMSValue/1000,3) + 
              ",\"Power\":"+ String(powerVals.ActivePowerValue/1000) + ",\"Apparent_Power\":"+ String(powerVals.ApparentPowerValue/1000) + 
              ",\"Temp\":"+ String(tempVal.TemperatureVal) + ",\"Frequency\":"+ String(pqVals.FrequencyValue,3) + ",\"PF\":"+ String(pqVals.PowerFactorValue) + 
              ",\"Phase_shift\":"+ String(pqVals.AngleValue_AV_AI) + ",\"Active_energy\":"+ String(realEnergy) + ",\"Reactive_energy\":"+ String(reactiveEnergy) + 
              ",\"Apparent_energy\":"+ String(apparentEnergy)+ ",\"HalfRMS_current\":"+ String(halfrmsVals.HalfCurrentRMSValue/1000) + ",\"THD_voltage\":"+ String(THDV) + 
              ",\"THD_current\":"+ String(THDI) + ",\"Memory_free\":"+ String(ESP.getFreeHeap()) + "}"; */
      Json =  "{\"Voltage\":"+ String(rmsVals.VoltageRMSValue/1000,3) + 
              ",\"Current\":"+ String(rmsVals.CurrentRMSValue/1000,3) + 
              ",\"Power\":"+ String(powerVals.ActivePowerValue/1000) + 
              ",\"PF\":"+ String(pqVals.PowerFactorValue) + 
              ",\"DevID\":" + DevID + "}"; 
    }
    else if (measMode == 1)
    {
      Json = "{\"Voltage\":"+ String(rmsVals.VoltageRMSValue/1000-1.05) + ",\"Current\":"+ String(rmsVals.CurrentRMSValue/1000*0.9434) +
              ",\"Power\":"+ String(powerVals.ActivePowerValue/1000) + ",\"Apparent_Power\":"+ String(powerVals.ApparentPowerValue/1000) + ",\"Temp\":"+ String(tempVal.TemperatureVal) + 
              ",\"Frequency\":"+ String(pqVals.FrequencyValue,3) + ",\"PF\":"+ String(pqVals.PowerFactorValue) + ",\"Phase_shift\":" + String(pqVals.AngleValue_AV_AI) + 
              ",\"Active_energy\":"+ String(realEnergy) + ",\"Reactive_energy\":"+ String(reactiveEnergy) + ",\"Apparent_energy\":"+ String(apparentEnergy)+ 
              ",\"HalfRMS_current\":" + String(halfrmsVals.HalfCurrentRMSValue/1000) + ",\"THD_voltage\":"+ String(THDV) + ",\"THD_current\":"+ String(THDI) + 
              ",\"Memory_free\":"+ String(ESP.getFreeHeap()) + "}";
    }
    
    events.send(Json.c_str(),"new_data" ,millis());
    //serverName1 = serverName1 + Json + "&apikey=" + apiKey; 
    //serverName1 = serverName1 + Json + "&apikey=" + apiKey; 
    

    HTTPClient http;
    http.begin(wifiClient, serverName1);
    Serial.println(serverName1);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(Json);

    //int httpResponseCode = http.GET();
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    if (httpResponseCode == 200) wifiWTD = 0;
    http.end();

  }
  else {
    Serial.println("WiFi Disconnected");
    //if (wifiWTD > 60) ESP.restart();//Wi-Fi watchdog
  }  
}

void resetADE9153A(void)
{
 digitalWrite(ADE9153A_RESET_PIN, LOW);
 delay(100);
 digitalWrite(ADE9153A_RESET_PIN, HIGH);
 delay(1000);
 Serial.println("Reset Done");

 long Igain = -2*134217728;

  delay(1000);

  /*SPI initialization and test*/
  bool commscheck = ade9153A.SPI_Init(SPI_SPEED,CS_PIN); //Initialize SPI
  if (!commscheck) {
    Serial.println("ADE9153A Shield not detected. Plug in Shield and reset the Arduino");
    
    while (!commscheck) {     //Hold until arduino is reset
      delay(1000);
      ArduinoOTA.handle();
    }
  }
  
  ade9153A.SetupADE9153A(); //Setup ADE9153A according to ADE9153AAPI.h
  /* Read and Print Specific Register using ADE9153A SPI Library */
  Serial.println(String(ade9153A.SPI_Read_32(REG_VERSION_PRODUCT), HEX)); // Version of IC
  ade9153A.SPI_Write_32(REG_AIGAIN, Igain);

}

void runLength(unsigned long seconds)
{
  unsigned long startTime = millis();
  
  while (millis() - startTime < (seconds*1000)){
    delay(blinkInterval);
    ade9153A.ReadAcalRegs(&acalVals);
    Serial.print("AICC: ");
    Serial.println(acalVals.AICC);
    Serial.print("AICERT: ");
    Serial.println(acalVals.AcalAICERTReg);
    Serial.print("AVCC: ");
    Serial.println(acalVals.AVCC);
    Serial.print("AVCERT: ");
    Serial.println(acalVals.AcalAVCERTReg);

  }
  HTTPClient http;
    
    // Your Domain name with URL path or IP address with path
    String serverName1 = "http://192.168.50.154:8080/input/post?node=esp32v4&fulljson={\"AICC\":"+ String(acalVals.AICC) + ",\"AICERT\":"+ String(acalVals.AcalAICERTReg) + 
                          ",\"AVCC\":"+ String(acalVals.AVCC) + ",\"AVCERT\":"+ String(acalVals.AcalAVCERTReg) + "}&apikey=" + apiKey;

    http.begin(wifiClient, serverName1);
    
    int httpResponseCode = http.GET();
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
      
    // Free resources
    http.end();
}

void measureWaveForm(int cycles)
{	

  float arrayV[cycles];
  float arrayI[cycles];

  unsigned int sampling_period_us;
  unsigned long microseconds;
  sampling_period_us = round(1000000*(1.0/samplingFrequency));
  microseconds = micros();

  for(int i=0; i < cycles; i++)
  {
      arrayV[i] = int32_t (SPI_Read_32(REG_AV_WAV));
      arrayI[i] = int32_t (SPI_Read_32(REG_AI_WAV));
      while(micros() - microseconds < sampling_period_us){
        //empty loop
      }
      microseconds += sampling_period_us;
  }
  
  for (int i=0; i < cycles; i++)
  {
      arrayI[i] = arrayI[i] / 1194486;
      arrayV[i] = arrayV[i] / 70711;

  }
  
  // Your Domain name with URL path or IP address with path
  String Vwaveform = "{";
  String Iwaveform = "{";
  for (int i=0; i < cycles; i++)//function for building JSON string to send data to Emoncms and web server
  {
      Vwaveform = Vwaveform + "\"" + String(i) + "\":" + String(arrayV[i],3);
      Iwaveform = Iwaveform + "\"" + String(i) + "\":" + String(arrayI[i],3);
      if (i < cycles - 1)//-1 is original solution
      {
        Vwaveform = Vwaveform + ",";
        Iwaveform = Iwaveform + ",";
      }
  }

  Vwaveform = Vwaveform + "}";
  Iwaveform = Iwaveform + "}";

  events.send(Vwaveform.c_str(),"new_voltage" ,millis());
  events.send(Iwaveform.c_str(),"new_current" ,millis());

  //Serial.println(Vwaveform);
  //Serial.println(Iwaveform);
}

uint32_t SPI_Read_32(uint16_t Address)
{
	uint16_t temp_address;
	uint32_t temp_highpacket;
	uint16_t temp_lowpacket;
	uint32_t returnData;
	
	digitalWrite(CS_PIN, LOW);
	
	temp_address = (((Address << 4) & 0xFFF0) + 8);
	SPI.transfer16(temp_address);
	temp_highpacket = SPI.transfer16(0);
	temp_lowpacket = SPI.transfer16(0);	
	
	digitalWrite(CS_PIN, HIGH);
	
	returnData = temp_highpacket << 16;
	returnData = returnData + temp_lowpacket;
	
	return returnData;
}

void configModeCallback (AsyncWiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void wifiManagerInit()
{
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  AsyncWiFiManager wifiManager1(&server2,&dns);

  display.println("Connecting to WiFi,  Please wait ...");
  display.display();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager1.setAPCallback(configModeCallback);
  
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if(!wifiManager1.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    #ifdef ESP32
      ESP.restart();
    #endif

    #ifdef ESP8266
      ESP.reset();
    #endif
    delay(1000);
  }
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Wi-Fi connected, IP:");
  display.println(WiFi.localIP());
  display.display();
  Serial.println(WiFi.localIP());

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  WiFi.setSleep(false);
  delay(2000);
}

void OTAinit()
{
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  if (DEBUG == false) {
    // Comment to: No authentication by default
    ArduinoOTA.setPassword(OTA_PASSDW);
    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  }

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


void calibrate()
{
    Serial.println("Autocalibrating Current Channel");
    ade9153A.StartAcal_AITurbo();
    runLength(60);
    ade9153A.StopAcal();
    Serial.println("Autocalibrating Voltage Channel");
    ade9153A.StartAcal_AV();
    runLength(60);
    ade9153A.StopAcal();
    delay(100);
    
    ade9153A.ReadAcalRegs(&acalVals);
    Serial.print("AICC: ");
    Serial.println(acalVals.AICC);
    Serial.print("AICERT: ");
    Serial.println(acalVals.AcalAICERTReg);
    Serial.print("AVCC: ");
    Serial.println(acalVals.AVCC);
    Serial.print("AVCERT: ");
    Serial.println(acalVals.AcalAVCERTReg);
    long Igain = (-acalVals.AICC-740.7) * 134217728; //622.4 for v3, 740.7 for v4
    long Vgain = (acalVals.AVCC-13289) * 134217728; //13283 for v3, 13289 for v4
    ade9153A.SPI_Write_32(REG_AIGAIN, Igain);
    ade9153A.SPI_Write_32(REG_AVGAIN, Vgain);
    
    Serial.println("Autocalibration Complete");
    delay(2000);
}

double doFFT(int cycles, int select)
{	

  float fftInput[cycles];
  float fftOutput[cycles];

  unsigned int sampling_period_us;
  unsigned long microseconds;
  sampling_period_us = round(1000000*(1.0/samplingFrequency));
  microseconds = micros();

  //float samplesMinusOne = (float(cycles) - 1.0); //Needed for window functions

  if (select == 0)
  {

    for(int i=0; i < cycles; i++)
    {
      fftInput[i] = int32_t (SPI_Read_32(REG_AV_WAV));
      while(micros() - microseconds < sampling_period_us){
        //empty loop
      }
      microseconds += sampling_period_us;
    }

    for (int i=0; i < cycles; i++)
    {
      //Window functions
      //float indexMinusOne = float(i);
      //float ratio = (indexMinusOne / samplesMinusOne);
      //float weighingFactor = 0.2810639 - (0.5208972 * cos(TWO_PI * ratio)) + (0.1980399 * cos(2 * TWO_PI * ratio));
      //float weighingFactor = 0.54 * (1.0 - cos(TWO_PI * ratio));// hann
      //float weighingFactor = 0.54 * (0.46 - cos(TWO_PI * ratio));// hamming
      float weighingFactor = 1;
      fftInput[i] = fftInput[i] / 70711 * weighingFactor;
    }
  }
  else if (select == 1)
  {
    for(int i=0; i < cycles; i++)
    {
      fftInput[i] = int32_t (SPI_Read_32(REG_AI_WAV));
      while(micros() - microseconds < sampling_period_us){
        //empty loop
      }
      microseconds += sampling_period_us;
    }
    for (int i=0; i < cycles; i++)
    {
      //window functions
      //float indexMinusOne = float(i);
      //float ratio = (indexMinusOne / samplesMinusOne);
      //float weighingFactor = 0.2810639 - (0.5208972 * cos(TWO_PI * ratio)) + (0.1980399 * cos(2 * TWO_PI * ratio)); // flat top
      //float weighingFactor = 0.54 * (1.0 - cos(TWO_PI * ratio)); // hann
      //float weighingFactor = 0.54 * (0.46 - cos(TWO_PI * ratio));// hamming
      float weighingFactor = 1;

      fftInput[i] = fftInput[i] / 1194486 * weighingFactor;
    }
  }
  float max_magnitude = 0;
  float fundamental_freq = 0;
  
  fft_config_t *real_fft_plan = fft_init(kordus2, FFT_REAL, FFT_FORWARD, fftInput, fftOutput);
  fft_execute(real_fft_plan);

  for (int k = 1 ; k < real_fft_plan->size / 2 ; k++)
  {
    /*The real part of a magnitude at a frequency is followed by the corresponding imaginary part in the output*/
    float mag = sqrt(pow(real_fft_plan->output[2*k],2) + pow(real_fft_plan->output[2*k+1],2))/1;
    float freq = k*1.0/(kordus2/samplingFrequency);
    if(mag > max_magnitude)
    {
        max_magnitude = mag;
        fundamental_freq = freq;
    }
  }

  int y0 = (fundamental_freq) * (kordus2/samplingFrequency*2);
  
  double calcTHD = 0;
  
  float y1 = abs(fftOutput[y0]); 
  for (int i = -3; i < 4; i++)
  {
    float temp = abs(fftOutput[y0+i]);
    if (temp > y1)
    {
      y1 = temp;
    }

  }
  float tegur = samplingFrequency/kordus2;
  float ysum = 0;

  for (int i = 2; i < ((samplingFrequency / 2 ) / (y0*tegur)); i++)
  {
    float yx = abs(fftOutput[y0*i]);
    for (int j = -3; j < 4; j++)
    {
      float temp = abs(fftOutput[y0*i+j]);
      if (temp > yx)
      {
        yx = temp;
      }
    }
    ysum = ysum + yx*yx;
  }

  calcTHD = sqrt(ysum)/y1*100;
  Serial.print("THD: ");
  Serial.print(calcTHD, 2);
  Serial.println(" %");

  String FFTV = "{";


  FFTV = FFTV + "\"" + String(0) + "\":" + String(abs(fftOutput[0])/cycles)+ ",";
  for (int i=1; i < (cycles >> 1); i=i+1)
  {
    
    FFTV = FFTV + "\"" + String(i) + "\":" + String(abs(fftOutput[i])/cycles*2);

    if (i < (cycles >> 1 )- 1)//-1 is original solution
    {
      FFTV = FFTV + ",";
    }
  }
  FFTV = FFTV + "}";

  fft_destroy(real_fft_plan);
  
  if (select == 0)
  {
    events.send(FFTV.c_str(),"new_VFFT" ,millis());
  }
  else if (select == 1)
  {
    events.send(FFTV.c_str(),"new_IFFT" ,millis());
  }

  return calcTHD;
}

void initWebServer ()
{
  // Web Server Root URL
  server2.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html", false, processor);
  });

  server2.serveStatic("/", SPIFFS, "/");
  
  // Request for the latest sensor readings
  server2.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "OK!"); 
  });

  // Request for voltage and current calibration
  server2.on("/calib", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "OK!");
    xTaskCreate(calibTask,"Calib",2048,NULL,1,&Task2);
  });

  // Request for voltage and current calibration
  server2.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "Reseting ESP!");
    ESP.restart();
  });

  // Send a GET request to <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
  server2.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    
    String inputMessage1 = "";
    String inputMessage2 = "";
    
    // GET input1 value on <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
      inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
      
      if ((inputMessage1 == "Waveb") && (inputMessage2.toInt() == 1) && (waveState == 0))
      {
        request->send(200, "text/plain", "Wave task started!");
        xTaskCreate(advTask,"Wave",20000,NULL,5,&Task1);
        waveState = 1;
      }

      else if ((inputMessage1 == "Waveb") && (inputMessage2.toInt() == 0) && (waveState == 1))
      {
        request->send(200, "text/plain", "Wave task killed!");
        vTaskDelete(Task1);
        waveState = 0;
      }

      if ((inputMessage1 == "Relayb") && (inputMessage2.toInt() == 1) && (relayState == 0))
      {
        request->send(200, "text/plain", "Load turned on!");
        digitalWrite(relay, LOW);
        relayState = 1;
        
      }

      else if ((inputMessage1 == "Relayb") && (inputMessage2.toInt() == 0) && (relayState == 1))
      {
        request->send(200, "text/plain", "Load turned off!");
        digitalWrite(relay, HIGH);
        relayState = 0;
        
      }

      else if ((inputMessage1 == "Modeb") && (inputMessage2.toInt() == 0))
      {
        request->send(200, "text/plain", "AC mode!");
        measMode = 0;
        ade9153A.SPI_Write_32(REG_CONFIG0,0x00000000);
      }

      else if ((inputMessage1 == "Modeb") && (inputMessage2.toInt() == 1))
      {
        request->send(200, "text/plain", "DC mode!");
        measMode = 1;
        ade9153A.SPI_Write_32(REG_CONFIG0,0x00000001);
      }
    }

    else if (request->hasParam(PARAM_INPUT_TRIGGER_COUNT) && request->hasParam(PARAM_INPUT_TRIGGER_LEVEL) && request->hasParam(PARAM_INPUT_TRIGGER_TIMEOUT) && request->hasParam(PARAM_INPUT_TRIGGER_SELECT))
    {
      Message1 = request->getParam(PARAM_INPUT_TRIGGER_COUNT)->value();
      Message2 = request->getParam(PARAM_INPUT_TRIGGER_LEVEL)->value();
      Message3 = request->getParam(PARAM_INPUT_TRIGGER_TIMEOUT)->value();
      Message4 = request->getParam(PARAM_INPUT_TRIGGER_SELECT)->value();
      
      xTaskCreate(triggerTask,"trigger",10000,NULL,6,&Task5);
      request->send(200, "text/plain", "Trigger task started!");
    }

    else {
      inputMessage1 = "No message sent";
      inputMessage2 = "No message sent";
    }

    Serial.print(inputMessage1);
    Serial.print(" - Set to: ");
    Serial.println(inputMessage2);
    
  });

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 10 seconds
    client->send("hello!", NULL, millis(), 10000);
  });
  
  server2.addHandler(&events);

  // Start server
  server2.begin();

if(!MDNS.begin("RocketSocket1")) {
     Serial.println("Error starting mDNS");
     return;
  }

 MDNS.addService("http", "tcp", 80);

}

int trigger(int cycles, float triggerLevel, unsigned long timeout, int select)
{
  unsigned int sampling_period_us;
  unsigned long microseconds;
  unsigned long milliseconds;
  float triggerBuffer[cycles];
  float triggerBuffer2[cycles];

  sampling_period_us = round(1000000*(1.0/samplingFrequency));
  int32_t triggerCode = 0;
  if (select == 0)
  {
    triggerCode = triggerLevel * 1194486; //for current
  }
  else if (select == 1)
  {
    triggerCode = triggerLevel * 70711; //for voltage
  }
  milliseconds = millis();
  microseconds = micros();

  int y = 0;
  int32_t  readValue = (SPI_Read_32(REG_AI_WAV));
  int32_t  readValue2 = (SPI_Read_32(REG_AV_WAV));
  triggerBuffer[y] = readValue;
  triggerBuffer2[y] = readValue;
  y++;
  delayMicroseconds(200);

  //Prefilling buffer
  while ((readValue < triggerCode && select == 0) || (readValue2 < triggerCode && select == 1) || y < cycles/2)
  {
    readValue = (SPI_Read_32(REG_AI_WAV));
    readValue2 = (SPI_Read_32(REG_AV_WAV));
    if ( y > cycles/2)
    {
      for (int i = 1; i < y + 1; i++)
      {
        triggerBuffer[i-1] = triggerBuffer[i];
        triggerBuffer2[i-1] = triggerBuffer2[i];
      }
      triggerBuffer[cycles/2] = readValue;
      triggerBuffer2[cycles/2] = readValue2;
    }
    else
    {
      triggerBuffer[y] = readValue;
      triggerBuffer2[y] = readValue2;
      y++;
    }

    if (millis() - milliseconds > timeout) return 1;

    while(micros() - microseconds < sampling_period_us)
    {
        //empty loop
    }
    microseconds += sampling_period_us;
  }
  
  for(int i=cycles/2+1; i < cycles; i++)
  {
      triggerBuffer[i] = int32_t (SPI_Read_32(REG_AI_WAV));
      triggerBuffer2[i] = int32_t (SPI_Read_32(REG_AV_WAV));
      while(micros() - microseconds < sampling_period_us)
      {
        //empty loop
      }
      microseconds += sampling_period_us;
  }
  for (int i=0; i < cycles; i++)
  {
      triggerBuffer[i] = triggerBuffer[i] / 1194486;
      triggerBuffer2[i] = triggerBuffer2[i] / 70711;
  }
  
  triggerData = "{";
  triggerData2 = "{";
  for (int i=0; i < (cycles); i++)
  {
    triggerData = triggerData + "\"" + String(i) + "\":" + String(triggerBuffer[i]);
    triggerData2 = triggerData2 + "\"" + String(i) + "\":" + String(triggerBuffer2[i]);
    if (i < (cycles)- 1)
    {
      triggerData = triggerData + ",";
      triggerData2 = triggerData2 + ",";
    }      
  }
  triggerData = triggerData + "}";
  triggerData2 = triggerData2 + "}";
  return 0;

}

void advTask( void * parameter ) 
{
  for (;;) 
  {
    measureWaveForm(200);
    THDV = doFFT(kordus2, 0);
    THDI = doFFT(kordus2, 1);
    vTaskDelay(15000 / portTICK_PERIOD_MS);
  }
}


void saveEnergyTask( void * parameter ) 
{
  for (;;) 
  {
    File file1 = SPIFFS.open("/RealEnergy.txt", FILE_WRITE);
    file1.print(String(realEnergy,3));
    Serial.println(String(realEnergy,3));
    file1.close();

    File file2 = SPIFFS.open("/ImagEnergy.txt", FILE_WRITE);
    file2.print(String(reactiveEnergy,3));
    Serial.println(String(reactiveEnergy,3));
    file2.close();

    File file3 = SPIFFS.open("/ApparentEnergy.txt", FILE_WRITE);
    file3.print(String(apparentEnergy,3));
    Serial.println(String(apparentEnergy,3));
    file3.close();

    vTaskDelay(60000 / portTICK_PERIOD_MS);
  }
}

void triggerTask( void * parameter ) 
{
  int status = trigger(Message1.toInt(), Message2.toFloat(), Message3.toInt(), Message4.toInt());
  for (;;) 
  {
    if (status == 0)
    {
      events.send(triggerData.c_str(),"new_trigger_current" ,millis());
      events.send(triggerData2.c_str(),"new_trigger_voltage" ,millis());
      Serial.println("Data sent");
    }
    vTaskDelete(Task5);
  }
}

void uptimeTask( void * parameter ) 
{
  for (;;) 
  {
    events.send(String(millis()/1000).c_str(),"uptime" ,millis());
    vTaskDelay(30000 / portTICK_PERIOD_MS);
  }
}

String processor(const String& var)
{
  if(var == "BUTTONPLACEHOLDER") //lülitite seisundi saamine
  {
    String buttons = "";
    buttons += "<h4>AC/DC mode switch</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"Modeb\" " + outputState(measMode) + "><span class=\"slider2\"></span></label>";
    buttons += "<h4>Show V/I Waveform, Voltage and Current FFT</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"Waveb\" " + outputState(waveState) + "><span class=\"slider\"></span></label>";
    buttons += "<h4>Toggle connected load</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"Relayb\" " + outputState(relayState) + "><span class=\"slider\"></span></label>";
    return buttons;
  }
  return String();
}

String outputState(int output)
{
  if(output == 1)
  {
    return "checked";
  }
  else 
  {
    return "";
  }
}

void readEnergyValues(void)
{
  File file1 = SPIFFS.open("/RealEnergy.txt");
  String buffer = "";
  if (!file1) 
  {
    Serial.println("There was an error opening the file1 for reading");
  }
  while(file1.available())
  {
    buffer += (char)file1.read();
  }
  Serial.println(buffer);
  realEnergy = buffer.toFloat();
  Serial.println(realEnergy);
  file1.close();

  File file2 = SPIFFS.open("/ImagEnergy.txt");
  buffer = "";
  if (!file2) 
  {
    Serial.println("There was an error opening the file2 for reading");
  }
  while(file2.available())
  {
    buffer += (char)file2.read();
  }

  Serial.println(buffer);
  reactiveEnergy = buffer.toFloat();
  Serial.println(reactiveEnergy);
  file2.close();

  File file3 = SPIFFS.open("/ApparentEnergy.txt");
  buffer = "";
  if (!file3) 
  {
    Serial.println("There was an error opening the file3 for reading");
  }
  while(file3.available())
  {
    buffer += (char)file3.read();
  }

  Serial.println(buffer);
  apparentEnergy = buffer.toFloat();
  Serial.println(apparentEnergy);
  file3.close();
}

void sendDataToDBTask( void * parameter ) 
{
  for (;;) 
  {
    if (wifiWTD > 60) ESP.restart();//Wi-Fi watchdog,
    wifiWTD++;
    readandwrite();//Uploading data to external server
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void OTATask( void * parameter ) 
{
  for (;;) 
  {
    ArduinoOTA.handle();//OTA update task
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void displayUpdateTask( void * parameter ) 
{
  for (;;) 
  {
    updateDisplay(); //Display update 
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void calibTask( void * parameter ) 
{
  for (;;) 
  {
    calibrate(); //voltage and current channel update
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    vTaskDelete(Task2);
  }
}

void displayInit()
{
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println("SSD1306 allocation failed");
    for(;;);
  }
  delay(2000);

  display.clearDisplay();
  display.setTextSize(1);             
  display.setTextColor(WHITE);        
  display.setCursor(0,0);
}


void updateDisplay()
{
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Live Data 1/3:");
  display.setCursor(0,15);

  double test1 = rmsVals.VoltageRMSValue/1000;
  double test2 = rmsVals.CurrentRMSValue/1000;
  double test3 = powerVals.ActivePowerValue/1000;
  double test4 = powerVals.ApparentPowerValue/1000;
  double test5 = pqVals.FrequencyValue;
  double test6 = pqVals.PowerFactorValue;

  int placeX1 = 94;
  int placeY1 = 1;
  int placeX2 = placeX1 +20;
  int placeY2 = placeY1;

  int placeX3 = 70;
  int placeY3 = 14;
  int placeX4 = placeX3 +20;
  int placeY4 = placeY3;

  int placeX5 = 82;
  int placeY5 = 27;
  int placeX6 = placeX5 +20;
  int placeY6 = placeY5;

  //Uptime
  long timeVar = millis()/1000;
  long secs = timeVar % 60;
  timeVar = (timeVar - secs) / 60;
  
  long mins = timeVar % 60;
  timeVar = (timeVar - mins) / 60;
    
  long hrs = timeVar % 24;
  timeVar = (timeVar - hrs) / 24;

  long days = timeVar; 

  delay(10);

  display.println("Voltage: "+ String(test1, 3) + " V");
  display.println("Current: "+ String(test2, 3)+ " A");
  delay(10);
  display.println("Power: "+ String(test3)+ " W");
  display.println("Ap. Power: "+ String(test4)+ " VA");
  delay(10);
  display.println("Freq: "+ String(test5, 3)+ " Hz");
  display.println("PF: "+ String(test6));
  delay(10);
  display.display();
  delay(5000);

  display.clearDisplay();
  delay(10);
  display.setCursor(0,0);
  display.println("Live Data 2/3:");
  display.setCursor(0,15);
  delay(10);
  display.println("Active energy: ");
  display.println(String(realEnergy) + " Wh");
  display.println("Reactive energy: ");
  display.println(String(reactiveEnergy) + " varh");
  display.println("Apparent energy: ");
  display.println(String(apparentEnergy) + " VAh");
  delay(5);
  display.display();
  delay(5000); 

  display.clearDisplay();
  delay(5);
  display.setCursor(0,0);
  display.println("Live Data 3/3:");
  display.setCursor(0,15);

  display.println("Ph_shift: "+ String(pqVals.AngleValue_AV_AI) + " deg");
  display.println("HalfRMS cur: "+ String(halfrmsVals.HalfCurrentRMSValue/1000, 3)+ " A");
  delay(10);
  display.println("THD_V: "+ String(THDV)+" %");
  display.println("THD_I: "+ String(THDI)+ " %");
  delay(10);
  display.println("Temp: "+ String(tempVal.TemperatureVal)+ " C");
  display.println("Free_mem: "+ String(ESP.getFreeHeap())+ " B");
  delay(10);
  display.display();
  delay(5000); 


  display.clearDisplay();
  delay(5);
  display.setCursor(0,placeY1);
  display.println("Operation mode: ");
  display.setCursor(placeX1,placeY1);
  display.println("AC");
  display.setCursor(placeX2,placeY2);
  display.println("DC");

  if (measMode == 0)
  {
    display.fillRect(placeX1-1, placeY1-1, 13, 9, INVERSE);//AC mode
  }
  else if (measMode == 1)
  {
    display.fillRect(placeX2-1, placeY2-1, 13, 9, INVERSE);//DC mode
  }
  
  display.setCursor(0,placeY3);
  display.println("Adv. mode: ");
  display.setCursor(placeX3,placeY3);
  display.println("ON");
  display.setCursor(placeX4,placeY3);
  display.println("OFF");

  if (waveState == 1)
  {
    display.fillRect(placeX3-1, placeY3-1, 13, 9, INVERSE); //Waveform + FFT off
  }
  else if (waveState == 0)
  {
    display.fillRect(placeX4-1, placeY4-1, 20, 9, INVERSE);//Waveform + FFT on
  }

  display.setCursor(0,placeY5);
  display.println("Load status: ");
  display.setCursor(placeX5,placeY5);
  display.println("ON");
  display.setCursor(placeX6,placeY6);
  display.println("OFF");

  if (relayState == 1)
  {
    display.fillRect(placeX5-1, placeY5-1, 13, 9, INVERSE); //Waveform + FFT off
  }
  else if (relayState == 0)
  {
    display.fillRect(placeX6-1, placeY6-1, 20, 9, INVERSE);//Waveform + FFT on
  }

  display.setCursor(0,placeY5 +13);
  display.println("Uptime:");  
  display.setCursor(0,placeY5 +26);
  display.println(String(days)+ "d "+ String(hrs)+ "h "+ String(mins)+ "min "+ String(secs)+ "s");
  delay(5);
  display.display();


}
