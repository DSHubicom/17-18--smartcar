/**************************************************************
 *
 * Based on
 * TinyGSM Getting Started guide:
 *   http://tiny.cc/tiny-gsm-readme
 * modified by Marino Linaje (22-Oct-2017)
 * primary to add some GPS functionality
 *
 **************************************************************/

// Select your modem:
//#define TINY_GSM_MODEM_SIM800
 #define TINY_GSM_MODEM_SIM808
// #define TINY_GSM_MODEM_SIM900

// Set serial for debug console (to the Serial Monitor, speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
// Use Hardware Serial on Mega, Leonardo, Micro
#define SerialAT Serial1

// or Software Serial on Uno, Nano
//#include <SoftwareSerial.h>
//SoftwareSerial SerialAT(2, 3); // RX, TX


//#define DUMP_AT_COMMANDS
#define TINY_GSM_DEBUG SerialMon

// Set phone numbers, if you want to test SMS and Calls
#define SMS_TARGET  "+34695866894"
//#define CALL_TARGET "+34xxxxxxxxx"

// Your GPRS credentials
// Leave empty, if missing user or pass
const char apn[]  = "YourAPN";
const char user[] = "";
const char pass[] = "";
String usuario = "empty";

#include <TinyGsmClient.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 53
#define RST_PIN 5
#define MOSI_PIN 51
#define MISO_PIN 50
#define SCK_PIN 52
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key; 
// Init array that will store new NUID 
byte nuidPICC[4];
int done = 0;
int first_time=0;

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

void setup() {
  // Set console baud rate
  SerialMon.begin(115200);
  delay(10);
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522 
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  // Set GSM module baud rate
  TinyGsmAutoBaud(SerialAT);
  delay(3000);
  
  while(usuario == "empty")
    checkNFC();

 

}

void loop() {

if (first_time == 0){
  DBG("Initializing modem...");
  
    // Unlock your SIM card with a PIN
  modem.simUnlock("8010");

  DBG("Waiting for network...");
  if (!modem.waitForNetwork()) {
    delay(10000);
    return;
  }
  first_time = 1;
}
  String modemInfo = modem.getModemInfo();
  DBG("Modem:", modemInfo);

  bool res;

  String ccid = modem.getSimCCID();
  DBG("CCID:", ccid);

  String imei = modem.getIMEI();
  DBG("IMEI:", imei);

  String cop = modem.getOperator();
  DBG("Operator:", cop);
  
  IPAddress local = modem.localIP();
  DBG("Local IP:", local);

  int csq = modem.getSignalQuality();
  DBG("Signal quality:", csq);

  // This is only supported on SIMxxx series
  String gsmLoc = modem.getGsmLocation();
  DBG("GSM location:", gsmLoc);

#if defined(TINY_GSM_MODEM_SIM808)
  float gps_lat, gps_lon, gps_speed=0;
  int gps_alt=0, gps_vsat=0, gps_usat=0, gps_year=0, gps_month=0, gps_day=0, gps_hour=0, gps_minute=0, gps_second=0;

  modem.enableGPS();
  DBG("Esperando senal de cobertura.....");
  while (!modem.getGPS(&gps_lat, &gps_lon, &gps_speed, &gps_alt, &gps_vsat, &gps_usat));
  DBG("GPS lat:", gps_lat);
  DBG("GPS lon:", gps_lon);
  DBG("GPS speed:", gps_speed);
  DBG("GPS alt:", gps_alt);
  DBG("GPS vsat:", gps_vsat);
  DBG("GPS usat:", gps_usat);

  while(!modem.getGPSTime(&gps_year, &gps_month, &gps_day, &gps_hour, &gps_minute, &gps_second));
  gps_hour=gps_hour+2; // UTC +1 MADRID 
  DBG("GPS date: ", gps_day , "/", gps_month ,"/", gps_year , "    ", gps_hour, ":", gps_minute, ":", gps_second);
  modem.disableGPS();
  
#endif
/*
 * Comprobaciones de persona, salidas de Cáceres, velocidad y uso en horas de madrugada. Se notifica mediante SMS.
 */

DBG("Realizando comprobaciones...");
#if defined(SMS_TARGET)
  if (usuario != "empty" && done == 0){
  res = modem.sendSMS(SMS_TARGET, String("La siguiente persona acaba de entrar en el coche: " + usuario + "y su posición es: " + gps_lat + " " + gps_lon) + imei);
  DBG("SMS:", res ? "OK" : "fail");
  done = 1;
  delay(2000);
  }
  if ((gps_lon > (-6.27)) && (gps_lon < (-6.44)) && (gps_lat > (39.41)) && (gps_lat < (39.53))){
  res = modem.sendSMS(SMS_TARGET, String("El siguiente usuario ha abandonado Cáceres: " + usuario + "y su posición es: " + gps_lat + " " + gps_lon) + imei);
  DBG("SMS:", res ? "OK" : "fail");
  delay(2000);
  }
  if (gps_speed > 130.00){
  res = modem.sendSMS(SMS_TARGET, String("El siguiente usuario ha excedido la velocidad: " + usuario) + imei);
  DBG("SMS:", res ? "OK" : "fail");
  delay(2000);
  }
  if (gps_hour > 00 && gps_hour < 06){
  res = modem.sendSMS(SMS_TARGET, String("El coche se está usando de madrugada, el usuario es: " + usuario + "y su posición es: " + gps_lat + " " + gps_lon) + imei);
  DBG("SMS:", res ? "OK" : "fail");
  delay(2000);
  }
  DBG("Comprobaciones realizadas...");
#endif


}

/*
 * Read NFC target data
 */
void checkNFC(){
  
   // Look for new cards
  if ( ! rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial())
    return;

  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));

  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return;
  }

  if (rfid.uid.uidByte[0] != nuidPICC[0] || 
    rfid.uid.uidByte[1] != nuidPICC[1] || 
    rfid.uid.uidByte[2] != nuidPICC[2] || 
    rfid.uid.uidByte[3] != nuidPICC[3] ) {
    Serial.println(F("A new card has been detected."));
    
  if (rfid.uid.uidByte[0] == 145 || 
    rfid.uid.uidByte[1] == 50 || 
    rfid.uid.uidByte[2] == 251 || 
    rfid.uid.uidByte[3] == 53 ) {
      usuario = "Sergio";
    } else if (rfid.uid.uidByte[0] == 197 || 
    rfid.uid.uidByte[1] == 125 || 
    rfid.uid.uidByte[2] == 201 || 
    rfid.uid.uidByte[3] == 85 ){
      usuario = "Donald";
    }
   
    // Store NUID into nuidPICC array
    for (byte i = 0; i < 4; i++) {
      nuidPICC[i] = rfid.uid.uidByte[i];
    }
   
    Serial.println(F("The NUID tag is:"));
    Serial.print(F("In hex: "));
    printHex(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
    Serial.print(F("In dec: "));
    printDec(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
  }
  else Serial.println(F("Card read previously."));

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

/**
 * Helper routine to dump a byte array as hex values to Serial. 
 */
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/**
 * Helper routine to dump a byte array as dec values to Serial.
 */
void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}
