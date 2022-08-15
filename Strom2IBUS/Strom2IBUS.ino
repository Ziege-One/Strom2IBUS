/*
   Strom2IBUS
   Ziege-One
    
 Arduino pro Mini 5v/16mHz w/ Atmega 328

 
 /////Pin Belegung////
 D0: 
 D1: 
 D2: Setup Pin
 D3: RX / TX Softserial IBUS Sensor
 D4: 
 D5: 
 D6: 
 D7: 
 D8: 
 D9: 
 D10: 
 D11: 
 D12: 
 D13: LED, um die SetupMode zu visualisieren
 
 A0: Eingang Spannung 0-5V
 A1: Eingang Strom 0-5V
 A2: 
 A3: 
 A4: 
 A5: 
 
 
 */
 
// ======== Strom2IBUS  =======================================

#include "iBUSTelemetry.h"
#include <EEPROM.h>

const float Version = 0.1; // Software Version

//Pinbelegung
volatile unsigned char TelePin = 3;
volatile unsigned char SetupPin = 2;
volatile unsigned char LedPin = 13;
volatile unsigned char SpannungPin = A0;
volatile unsigned char StromPin = A1;



iBUSTelemetry telemetry(TelePin); // IBUS Telemetry Pin zusweisen
#define UPDATE_INTERVAL 500       // Telemetrie update interval
uint32_t prevMillis = 0;          // Speicher Zelle für UPDATE_INTERVAL berechnung

//Zeitwerte für die Berechnung der genutzten Kapazität 
unsigned long current_time = 0;
unsigned long last_time = 0;
float time_elapsed = 0.0f;

//Berechnen Sie diesen Skalierungsfaktor hier einmal, um Zyklen zu sparen
float secound_scale = 1000.0f / 3600.0f;  

//Strom und Spnnugswerte
float Volt = 0.0f;
float VoltDigi = 0.0f; 
float Current = 0.0f;
float CurrentDigi = 0.0f;
float Capacity_Used = 0.0f;

// Messbereiche im Eprom (init Werte)
int Volt_Offset = 0;  
int Volt_SCALE = 2500;
int Current_Offset = 0;
int Current_SCALE = 2500;

// Zum Speichern von Einstellungen im EEPROM
#define adr_eprom_test 0                 // Für den Test zum 1. Mal init des Arduino (erstes Einschalten
#define adr_eprom_volt_offset 2          // Volt Offset in mV 
#define adr_eprom_Volt_SCALE 4           // Volt SCALE in /10 mV/V
#define adr_eprom_current_offset 6       // Current Offset in mV 
#define adr_eprom_Current_SCALE 8        // Current SCALE in /10 mV/A

// ======== Setup  =======================================
void setup()
{
  pinMode(SetupPin, INPUT_PULLUP);  // Setuppin
  pinMode(LedPin, OUTPUT);          // LED Pin
  
  // Test zum ersten Mal init des Arduino (erstes Einschalten)
  int test = read_eprom(adr_eprom_test);
  if (test != 123)
  {
    write_eprom(adr_eprom_test,123);
    write_eprom(adr_eprom_volt_offset,Volt_Offset);
    write_eprom(adr_eprom_Volt_SCALE,Volt_SCALE);
    write_eprom(adr_eprom_current_offset,Current_Offset);
    write_eprom(adr_eprom_Current_SCALE,Current_SCALE);
  }
  
  // Gespeicherte Werte aus EEPROM lesen
  Volt_Offset = read_eprom(adr_eprom_volt_offset);
  Volt_SCALE = read_eprom(adr_eprom_Volt_SCALE);
  Current_Offset = read_eprom(adr_eprom_current_offset);
  Current_SCALE = read_eprom(adr_eprom_Current_SCALE);

  if (digitalRead(SetupPin)) //Normal Modus
  {
    telemetry.begin(); // starte Telemetrie

    telemetry.addSensor(IBUS_MEAS_TYPE_EXTV);       //Spannungs Sensor
    telemetry.addSensor(IBUS_MEAS_TYPE_BAT_CURR);   //Strom Sensor
    telemetry.addSensor(IBUS_MEAS_TYPE_FUEL);       //Verbrauchter Strom Sensor
  }
  else  //Setup
  {
    Serial.begin (115200); // Serialeschnittstelle starten
    delay(20);
    SetupMenu();   
  }
}

// ======== Loop  =======================================
void loop()
{
  if (digitalRead(SetupPin)) //Normal Modus
  {
    updateValues();     //Aktualisiere Telemetrie
    telemetry.run();    //Telemetrie Aufruf
  }
  else 
  // Setup Modus
  { 
    uint8_t In = Serial.read(); 
    if(In=='A') {Volt_Offset = Serial.parseInt();       SetupMenu();} 
    if(In=='B') {Volt_SCALE = Serial.parseInt();        SetupMenu();}  
    if(In=='C') {Current_Offset = Serial.parseInt();    SetupMenu();}
    if(In=='D') {Current_SCALE = Serial.parseInt();     SetupMenu();}
    if(In=='S') {                                       Menu_Save();}
    if(In=='Z') {                                       SetupMenu();}
  }
}

void updateValues()
{
  uint32_t currMillis = millis();

  if (currMillis - prevMillis >= UPDATE_INTERVAL) { // Telemetrie Daten alle x Millisekunden schreiben
    prevMillis = currMillis;

    ReadSensor();   // Sensoren einelesen
    
    telemetry.setSensorValueFP(1, Volt); // IBUS_MEAS_TYPE_EXTV
    telemetry.setSensorValueFP(2, Current); // IBUS_MEAS_TYPE_BAT_CURR
    telemetry.setSensorValueFP(3, Capacity_Used); // IBUS_MEAS_TYPE_FUEL
  }
}

void SetupMenu()
{
  ReadSensor(); // Sensoren einelesen
  ReadSensor(); // Sensoren einelesen nochmal
  // Setup Modus
  Serial.print("\nStrom2IBUS Version "+String(Version)+"\n"); 
  Serial.print("Setup Modus:\n"); 
  Serial.print("\nSpannung Messung:\t"+String(Volt)+"V\n");
  Serial.print("Storm Messung:\t\t"+String(Current)+"A\n");
  Serial.print("\nAuswahl: (z.B. A1000 -> 1000mV offset)\n");
  Serial.print("A Spannung offset:\t"+String(Volt_Offset)+"mV\n");
  Serial.print("B Spannung Scale:\t"+String(Volt_SCALE)+" /10 mV/V\n");
  Serial.print("C Strom offset:\t\t"+String(Current_Offset)+"mV\n");
  Serial.print("D Strom Scale:\t\t"+String(Current_SCALE)+" /10 mV/A\n");
  Serial.print("S Speichern in EEprom\n");
  Serial.print("Z Hauptmenu\n");
  digitalWrite(LedPin,HIGH);  // LED an
  delay(20);
}

void Menu_Save()
{
  Serial.print(F("In EEprom gespeichert\n"));
  write_eprom(adr_eprom_test,123);
  write_eprom(adr_eprom_volt_offset,Volt_Offset);
  write_eprom(adr_eprom_Volt_SCALE,Volt_SCALE);
  write_eprom(adr_eprom_current_offset,Current_Offset);
  write_eprom(adr_eprom_Current_SCALE,Current_SCALE);
}

void ReadSensor(){
  // Spannung + Strom
      
for (uint8_t i = 0; i < 50; i++)    // 50 Messwerte aufnehmen
  {
    VoltDigi += analogRead(SpannungPin);    
    CurrentDigi += analogRead(StromPin); 
  }
   
  VoltDigi = VoltDigi / 50;       // Durchschnitt der Messungen
  CurrentDigi = CurrentDigi / 50;
     
  Volt = (VoltDigi - (0.2048 * Volt_Offset))* ((488281.25 / Volt_SCALE) * 0.0001);            // Skalieren
  Current = ((((CurrentDigi * 4.8828125) - Current_Offset)) * 10) / Current_SCALE;

  // Nicht unter 0
  if (Volt < 0) Volt = 0.0;
  if (Current < 0) Current = 0.0;
    
  // Kapazität
  current_time = millis();
  //Calculate the used Capacity in [mAh]
  //Calculate elapsed Time in sec first
  time_elapsed = (float)(current_time - last_time)/1000.0f;
  //Now the used capacity
  Capacity_Used += Current * secound_scale * time_elapsed;
  last_time = current_time;
}  

//EEprom lesen 
uint16_t read_eprom(int address)
{
  return  (uint16_t) EEPROM.read(address) * 256 + EEPROM.read(address+1);
}

//EEprom schreiben 
void write_eprom(int address,uint16_t val)
{
  EEPROM.write(address, val  / 256);
  EEPROM.write(address+1,val % 256 );
}
