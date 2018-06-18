/*  Programmable Arduino based stompbox looper / switching system with Midi output
 *  Programmed by Patrick Klaassen (patrick.klaassen@gmail.com)
 *  Photo's on Google Photos : https://goo.gl/photos/JYGqpnPPz51WmjSG7
 *  Short introduction video on Youtube: https://youtu.be/dVaR2lphJOY
 *  
 *  Project was based on the projects by CarraN and Pascal Paquay
 *  
 *  I removed about 70% of the original code and added a lot of new stuff
 *  
 * 
 * Version 0.1  uses a 3 way switch to determine the mode
 * Version 0.2  Removed 3 way switch mode selection and replaced it with momentary mode switch to switch between looper and preset mode
 * Version 0.3  uses a single output for the looper leds LED and relays
 * Version 0.5  cleaned up code and restored memory 8
 * Version 0.6  removal of relay leds, moved indicators to bottom row of LCD, saves drilling holes in enclosure
 * Version 0.7  introduction of amp channel switching TRS tip ring sleeve for controlling Bugera V20 tube amp
 * Version 0.8  adding bank up and down buttons
 * Version 0.9  exchanged row and col in the keymap
 * Version 0.10 Improved bank handling
 * Version 0.11 DJ - added support for i2c looper and preset led control
 *              Added support for 4x20 i2c lcd display
 *              Added support for dedicated loop buttons
 * Version 0.12 Removed mode indicator LED's
 *              Added midi support for EVH 5150III, Flashback X4 and Strymon BigSky, using a configurable mechanism to easily support 
 *              new pedals when added to the pedal board
 *              Tried to use semi configurable mechanism for midi support using array's to store all the midi information for each device
 * Version 0.12 Added SwitchOrder to solve double audio boost issue with boost pedal and crunch channel activated during switch from crunch mode to clean mode
 * Version 0.13 Improved switch order mode
 * Version 0.14 Cleaned up the code
 *
 * New Fork: Adding 4 more loops*
 * Version  0.1 Initial version adding 4 loops more
 * 
 * Functions and subroutines
 * setup()
 * memoryDump()
 * initLEDs()
 * midiProg(byte status, int data)
 * setLCDChannel()
 * setLCDAmpSettings()
 * setSavePresetState(int led)
 * memory(int addr, int led)
 * checkSaveState(int led, char key)
 * writeOut(int relay)
 * mute()
 * readPreset(int addr, int pcNum, int led)
 * switchLoops(int memValue)
 * writeMidi(int addr) 
 * getAddress(int channel)
 * getAmpSetting(int ampChannel)
 * handleLoopKeyEvent(int channel)
 * handlePresetKeyEvent(int channel)
 * changeDeviceMode(int mode)
 * showLCDBankMode()
 * handleAmpBankEvent()
 * keypadEvent()
 * showTimer()
 * loop()
 */

#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Keypad.h>

// Set LCD defaults
LiquidCrystal_I2C lcd(0x3f, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address
//LiquidCrystal_I2C lcd(0x20, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // When simulating in Proteus use this one

// Set I2C IO addresses
#define IO_ADDR_Presets 0x38
#define IO_ADDR_Loops 0x39
#define IO_ADDR_Loops2 0x3A

const String strVersion="0.13";

/* LiquidCrystal_I2C  lcd(0x27,16,2); 
 * set the LCD address to 0x27 for a 16 chars and 2 line display. 
any other display can be used,just change parameters. */
// Define matrix rows and cols
const byte rows = 2; 
const byte cols = 12; /*change it the same value as numberOfPedal variable */
char keys[rows][cols] = {
{'a','b','c','d','e','f','g','h','i','j','k','l'}, // first row contains presets followed by functions
{'m','n','o','p','q','r','s','t','u','v','w','x'}  // second row contains loops u v w x are not used
// Mapping of matrix values
// a - preset/channel 1
// b - preset/channel 2
// c - preset/channel 3
// d - preset/channel 4
// e - preset/channel 5
// f - preset/channel 6
// g - preset/channel 7
// h - preset/channel 8
// i - Mode switch
// j - Mute on/off
// k - Bank up/midi (In older version also used for amp channel swicthing)
// l - Bank down (in older version also used for amp reverb switching)
// m - loop 1
// n - loop 2
// o - loop 3
// p - loop 4
// q - loop 5
// r - loop 6
// s - loop 7
// t - loop 8
// u - loop 9
// v - loop 10
// w - loop 11
// x - loop 12

};

int intPrevMem = 0; // Stores previous preset pickup, used to check boost pedal for determining the order of switching Midi first or pedals first
// Mode constants, used for switching modes
const int PRESETMODE = 0;
const int PROGRAMMODE = 1;
const int STOREMODE = 2;
const int BANKMODE = 3;
const int MIDIMODE = 4;
const int ORDERMODE = 5; // determines the order in with the pedals and midi devices are switched

// Var for storing mute mode, 0 for not active, 1 for active and 2 for tilt ;-) 
int muteMode = 0;

// midi settings
const int MIDIAMP = 0; // Position of amp in midi arrays
const int MIDIFB = 1;  // Position of Flashback in midi arrays
const int MIDIBS = 2;  // Position of Bigsky in midi arrays
const int MIDIMB = 3;  // Position og Mobius in midi arrays
String strMidiDevices[4] = {"              ","            ","              ","               "}; // Name tage used when programming midi settings for the devices
String strMidiDevicesShort[4] = {" "," "," "," "};  // Short names used in preset mode to display the presets midi device value
const int intMidiMaxValue[4] = {2,2,127,127};  // Maximum value per midi device
const int intMidiDeviceChannels[4] = {2,0,1,3};  // Midi address (channel) for each device
int intMidiValues[4]= {0,0,0,0};  // Current midi values per device
int intCurMidi = 0;
int intCurMidiValue = 0;
// Determines the order for switching midi or loops first
int intCurSwitchOrderValue = 0; // 0 is loop then midi, 1 is midi then loop

// Arduino I/O pins used for the button marix
byte rowPins[rows] = {22,23}; // 22 preset+controls, 23 loop
byte colPins[cols] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};  //1,2,3,4,5,6,7,8,mode,mute,up/reveb,down/channel  Pin 9 was 36 before.
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, rows, cols);
// I/O ports used for the loop relays
int relayPin[12] = {32,31,30,29,28,27,26,25,35,36,37,38};
char* relayName[12] = {"NEO Lesley", "EQ", "Black Star", "RC Booster", "Corona", "Flashback", "FX 7" , "FX 8", "FX 9", "FX 10", "FX 11" , "FX12"}; // names of the loop pedals, no longer used in LCD
// Names of the presets, presented in the LCD when activated
char* presetTextA[8] = {"      Bank A-1", "      Bank A-2", "      Bank A-3", "      Bank A-4", "      Bank A-5", "      Bank A-6", "      Bank A-7" , "      Bank A-8"};
char* presetTextB[8] = {"      Bank B-1", "      Bank B-2", "      Bank B-3", "      Bank B-4", "      Bank B-5", "      Bank B-6", "      Bank B-7" , "      Bank B-8"};
char* presetTextC[8] = {"      Bank C-1", "      Bank C-2", "      Bank C-3", "      Bank C-4", "      Bank C-5", "      Bank C-6", "      Bank C-7" , "      Bank C-8"};
// Variables for storing temp values of LED's 
int intPresetLEDs=0;
int intLoopLEDs=0;
int intPresetLEDsPrev=0;
int intLoopLEDsPrev=0;

// I/O ports used for addiotional relays
int muteRelay = 33;  // Mute relay for switching the first loop from signal to earth 
int muteRelay2 = 24;  // Mute relay for earthing amp out
int tuneRelay = 31;

int ampReverbRelay = 34;
int ampGainRelay = 33;
// Not implemented reading of current values from the amp controls
int ampReverbPin = A0;
int ampGainPin = A1;

int ampReverbValue = HIGH;
int ampGainValue = LOW;
int i;

int numberOfPedal = 12; /*adapt this number to your needs = number of loop pedals */
int saveState = 0; // 0 - no save active, 1 - waiting for save, 2 - processed save
int deviceMode = PRESETMODE; // 0 = preset mode, 1 = looper mode, 2 = store preset mode, 3 = change bank mode
int holdProcessed=0; // used for detecting hold state of mode switch (for switching to store mode)
int currentPreset = -1; // Store the current preset number, used when switching back to preset mode
int currentBank = 0; // allowed values 0, 100 or 200 for bank a, b and c, depending on the Arduino you use you can extend the numer of banks 
// The address space is as follows
// 10 contains the settings for the first preset in bank a
// 20 contains the settings for the second pedal up to 80 for the eighth pedal in bank a
// 110 contains the settings for the first preset in bank b up to 180 for the eighth in bank b
// 210 contains the settings for the first preset in bank c up to 180 for the eighth in bank c
// You can extend the banks
int newBank = 0; //Used for changing banks till setting is confirmed
unsigned long previousMillis = 0; // timer is used for blinking led's when saving preset
unsigned long previousReverbMillis = 0; // timer is used for disabling Reverb relay (the Bugera amp uses a mementary swicth to switch reverd)
unsigned long previousGainMillis = 0; // timer is used for disabling Gain relay (the Bugera amp uses a mementary swicth to switch between channels)
const long interval = 150; // interval at which the leds will blink
int previousRelayState[12]={LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW}; 
int previousButtonLEDState[12]={LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW};

unsigned long startMillis = 0; // timer is used for calculating event length, used for perfomance testing 

boolean debug = true; // When true the serial monitor is used to communicate various values and states

/******************************************************/

char* presetText()
{
    if (currentBank==0) { return presetTextA[currentPreset]; }
    if (currentBank==100) { return presetTextB[currentPreset]; }
    if (currentBank==200) { return presetTextC[currentPreset]; }
    return "Empty";
}

void setup()
{
  Serial.begin(9600);
  pinMode(13, INPUT);
  //  Set MIDI baud rate:
  Serial1.begin(31250);
  
  pinMode(muteRelay, OUTPUT);
  pinMode(muteRelay2, OUTPUT);
  digitalWrite(muteRelay,LOW); // set Mute mode all sound is muted till setuyp is finished
  digitalWrite(muteRelay2,LOW); // set Mute mode all sound is muted till setuyp is finished
  //pinMode(tuneRelay, OUTPUT);
  //digitalWrite(tuneRelay,LOW); // Removed from last build of the Switch8

  //Serial.begin(9600); /* for midi communication - pin 1 TX  WAS  Serial.begin(31250);*/
  lcd.begin (20,4); // for 20 x 4 LCD module
  lcd.setBacklightPin(3,POSITIVE); 
  lcd.setBacklight(HIGH);
  lcd.setCursor(0,0);
  lcd.print("  Nick's - GigRig");
  lcd.setCursor(0,1);
  lcd.print("Guitar pedal looper ");

  // Set amp in default mode
  pinMode(ampReverbRelay, OUTPUT);
  digitalWrite(ampReverbRelay,HIGH);
  pinMode(ampGainRelay, OUTPUT);
  digitalWrite(ampGainRelay,HIGH);

  // Set pin modes for relays and set default value to bypass all pedals
  for(i=0; i<numberOfPedal; i++)
  {
    pinMode(relayPin[i], OUTPUT);
    digitalWrite(relayPin[i],HIGH); //pullup all relay outputs in case off low level relayboard
  }
  initLEDs();
  delay(30);
  keypad.addEventListener(keypadEvent); //add an event listener for this keypad, meaning all the buttons on the Switch12
  keypad.setHoldTime(2000);
  readPreset(10, 1, 0); // Load default preset number 1 from bank a
  mute();
//  ampReverbValue = getAmpSetting(ampReverbPin);
//  ampGainValue = getAmpSetting(ampGainPin);
  
  if (debug) Serial.println("Init done");
}

void memoryDump()
{
  // Create dump of memory and write this to the console
  // Create looper to loop through the memory space from address 0 to 300
  int intMemory;
  if (debug) Serial.println("Bank A");
  for(i=0; i<99; i++)
  {
    // Read value as int from memory, value can be between 0-255
    intMemory = EEPROM.read(i);
    // Write memory to console
    if (debug) Serial.print("addr: ");
    if (debug) Serial.print(i);
    if (debug) Serial.print(" - ");
    if (debug) Serial.println(intMemory);
  }
  if (debug) Serial.println("Bank B");
  for(i=100; i<199; i++)
  {
    // Read value as int from memory, value can be between 0-255
    intMemory = EEPROM.read(i);
    // Write memory to console
    if (debug) Serial.print("addr: ");
    if (debug) Serial.print(i);
    if (debug) Serial.print(" - ");
    if (debug) Serial.println(intMemory);
  }
  if (debug) Serial.println("Bank C");
  for(i=200; i<299; i++)
  {
    // Read value as int from memory, value can be between 0-255
    intMemory = EEPROM.read(i);
    // Write memory to console
    if (debug) Serial.print("addr: ");
    if (debug) Serial.print(i);
    if (debug) Serial.print(" - ");
    if (debug) Serial.println(intMemory);
  }
  // done
}


void initLEDs()
{
  // initLEDs tests all LED's so the user can see if all is working (and it looks nice)
  static unsigned char data = 0x01;  // data to display on LEDs
  static unsigned char direc = 1;    // direction of knight rider display
  int x = 0;
  for (x=0; x<2;x++)
  {
    for(i=0; i<16; i++)
    {
      // send the data to the LEDs
      Wire.beginTransmission(IO_ADDR_Presets);
      Wire.write(~data);
      Wire.endTransmission();
      Wire.beginTransmission(IO_ADDR_Loops2);
      Wire.write(~data);
      Wire.endTransmission();
      Wire.beginTransmission(IO_ADDR_Loops);
      Wire.write(~data);
      Wire.endTransmission();
      delay(40);  // speed of display
      // shift the on LED in the specified direction
      if (direc) {
        data <<= 1;
      }
      else {
        data >>= 1;
      }
      // see if a direction change is needed
      if (data == 0x80) {
        direc = 0;
      }
      if (data == 0x01) {
        direc = 1;
      }
    }
  }
  // LEDs of
  Wire.beginTransmission(IO_ADDR_Presets);
  Wire.write(~0);
  Wire.endTransmission();
  Wire.beginTransmission(IO_ADDR_Loops2);
  Wire.write(~0);
  Wire.endTransmission();
  
  Wire.beginTransmission(IO_ADDR_Loops);
  Wire.write(~0);
  Wire.endTransmission();
}

/*********************************************************/

//  Send a two byte midi message  
void midiProg(char status, int data ) {
  // This routine is used to send the midi program change commands to the midi devices
  if (debug) Serial.println("Write midi data");
  Serial1.write(status);
  Serial1.write(data);
}

/*********************************************************/

/********************************************************/

void setLCDChannel()
{
  // Dislays the channel status on the LCD, channels are indicated by their numbers, function now controls I2C led's
  // Excemple __3_567_
  if (debug) Serial.println("setLCDChannel");
  int intVal = LOW;
  int intPos = 2;
  intLoopLEDs =0;
  for(i=0; i<numberOfPedal; i++)
  {
    intPos = intPos+1;
    //if (debug) {Serial.print("Pos ");Serial.println(intPos);} 
    intVal = digitalRead(relayPin[i]);
    int intAdd = intVal<<(i);
    intLoopLEDs = intLoopLEDs + intAdd;
  }
  // if (debug) {Serial.print("Write value to lcd "); Serial.println(intLoopLEDs);}
  // Write value to loop I2C extension board
  
  byte LoByte = (intLoopLEDs & 0x00FF);
  byte HiByte = (((intLoopLEDs) >>8) & 0x00FF);
  
  Wire.beginTransmission(IO_ADDR_Loops);
  Wire.write(~LoByte);
  Wire.endTransmission();
  Wire.beginTransmission(IO_ADDR_Loops2);
  Wire.write(~HiByte);
  Wire.endTransmission();

//  setLCDAmpSettings();
  showLCDBankMode();
}

void setLCDAmpSettings()
{
  // Dislays the Amp reverb and channel status on the LCD, in last version no longer used
  // Excemple __3_567_ RC
  lcd.setCursor(12,3);
  if (ampReverbValue == HIGH)
  {
    //lcd.print(char(255));
    lcd.print("R");
  }
  else
  {
    lcd.print("_");
  }
  
  lcd.setCursor(13,3);
  if (ampGainValue == HIGH)
  {
    //lcd.print(char(255));
    lcd.print("C");
  }
  else
  {
    lcd.print("_");
  }
  
}

void setSavePresetState(int led)
{
  // sub is launched when holding the mode button to store a preset
  if (debug) Serial.println("Store preset key detected");
  // Write value to presets IO
  intPresetLEDs = 1<<led;
  Wire.beginTransmission(IO_ADDR_Presets);
  Wire.write(~intPresetLEDs);
  Wire.endTransmission();
  
  saveState = 1;
  if (debug) Serial.println("Set save state = 1");
}

void memory(int addr, int led)
{
  // Memory layout for each preset is the same and as follows:
  // 0 is loop settings 0-256 0x00000000 to 0x11111111 each bit represents a guitar pedal loop
  // 1 is the other 4 loop settings 0x00000000 to 0x00001111
  // For the midi devices only the program change code is stored. In the current memory model we have space for 8 midi devices
  // At the moment I only use three
  // 1 midi amp 0-2
  // 2 midi fb 0-2
  // 3 midi bs 0-127
  // 4 midi mb 0-127
  // 5-8 midi reserved
  // 9 switch order  value, this makes it possible to control the order of the switching of the midi or loops
  // 0-90 contain the preset info for the eight presets in bank A/0 (0 preset 1, 10 preset 2, 20 preset 3 etc.)
  // 100-190 contain the preset info for the eight presets in bank B/1 (100 preset 1, 110 preset 2 etc.)
  // 200-290 contain the preset info for the eight presets in bank C/2
  // Depending on the Arduino used you can add banks till you run out of memory
  
  if (debug) Serial.println("Store setting");
//  EEPROM.write((addr), intLoopLEDs);
  byte LoByte = (intLoopLEDs & 0x00FF);
  byte HiByte = (((intLoopLEDs) >>8) & 0x00FF);
  EEPROM.write((addr), LoByte);
  EEPROM.write((addr + 1), HiByte);
  
  // Store Midi settings, get the values from the midi values array for each of the devices
  EEPROM.write((addr) + 2 + MIDIAMP, intMidiValues[MIDIAMP]);
  EEPROM.write((addr) + 2 + MIDIFB, intMidiValues[MIDIFB]);
  EEPROM.write((addr) + 2 + MIDIBS, intMidiValues[MIDIBS]);
  EEPROM.write((addr) + 2 + MIDIMB, intMidiValues[MIDIMB]);
  // Reserve room for additional midi devices
  
  // Store switch order value
  EEPROM.write((addr) + 9, intCurSwitchOrderValue);

  // store amp settings, in the last version of the hardware this is not incorporated
//  EEPROM.write((addr) + 8, ampReverbValue);
//  EEPROM.write((addr) + 9, ampGainValue);
  
  // Update LCD to show status of save action
  lcd.setCursor(0,2);
  lcd.print("Preset ");
  lcd.print(led + 1);
  lcd.print(" stored         ");
  // Stop blinking preset leds
  intPresetLEDs = 0;  
  Wire.beginTransmission(IO_ADDR_Presets);
  Wire.write(~intPresetLEDs);
  Wire.endTransmission();
  // Show selected preset led
  intPresetLEDs = 1<<led;
  Wire.beginTransmission(IO_ADDR_Presets);
  Wire.write(~intPresetLEDs);
  Wire.endTransmission();
  
  if (debug) Serial.println("Preset stored");
  saveState = 2;  
  delay(200);
  changeDeviceMode(PROGRAMMODE);
}

void checkSaveState(int led, char key)
{
  // Check if save state should be cancelled
  if (debug) Serial.println("Check save state");
  if (saveState==1)
  {
    if (debug) Serial.println("Canceling save state");
    // Disable blinking preset LED's
    intPresetLEDs = 0;
    for(i=0; i<numberOfPedal; i++)
    {
      intPresetLEDs  = intPresetLEDs  + !digitalRead(relayPin[i])* 1<<(i+1); 
    }
    Wire.beginTransmission(IO_ADDR_Presets);
    Wire.write(~intPresetLEDs);
    Wire.endTransmission();
  }
  saveState = 0;  
}

/*********************************************************/

void writeOut(int relay) 
{
  // Toggle single loop value
  int intChannelVal = LOW;
  digitalWrite(relayPin[relay], !digitalRead(relayPin[relay]));
}

void mute() 
{
  // Set mute for tuning and pauze
  if (muteMode==0)
  {
    // Switch Mute relay
    digitalWrite(muteRelay,LOW); 
    digitalWrite(muteRelay2,LOW); 
    // digitalWrite(tuneRelay,LOW);
    // Get current state of Relays and button LEDs
    for(i=0; i<numberOfPedal; i++)
    {
      previousRelayState[i] = !digitalRead(relayPin[i]);
      previousButtonLEDState[i] = !previousRelayState[i]; //digitalRead(ledPin[i]);
    }
    for(i=0; i<numberOfPedal; i++)
    {
      digitalWrite(relayPin[i], HIGH);
    }
    muteMode = 1;
    lcd.setCursor(0,1);
    lcd.print("     Mute mode           ");
  }
  else
  {
    if (muteMode==2)
    {
      intPresetLEDs = intPresetLEDsPrev;
      intLoopLEDs = intLoopLEDsPrev;      
    }
    // reset previous state of Relays and button LEDs
    for(i=0; i<numberOfPedal; i++)
    {
      digitalWrite(relayPin[i], !previousRelayState[i]);
    }
    delay(10);
    // Disable mute relay
    // digitalWrite(tuneRelay,HIGH);
    digitalWrite(muteRelay,HIGH); 
    digitalWrite(muteRelay2,HIGH);
    if (deviceMode==PRESETMODE) // Preset mode
    {
      lcd.setCursor(0,1);
      lcd.print("    Preset mode         ");
      lcd.setCursor(0,2);
      lcd.print("                   ");
      lcd.setCursor(0,2);
      lcd.print(presetText());
    }
    else // looper mode
    {
      lcd.setCursor(0,1);
      lcd.print("    Program mode        ");
    }
    muteMode = 0;
    setLCDChannel();
    // Write value to presets IO
    Wire.beginTransmission(IO_ADDR_Presets);
    Wire.write(~intPresetLEDs);  
    Wire.endTransmission();
    // Write value to presets IO
    byte LoByte = (intLoopLEDs & 0x00FF);
    byte HiByte = (((intLoopLEDs) >>8) & 0x00FF);
  
    Wire.beginTransmission(IO_ADDR_Loops);
    Wire.write(~LoByte);
    Wire.beginTransmission(IO_ADDR_Loops2);
    Wire.write(~HiByte);
    Wire.endTransmission();
  }
}

/*********************************************************/
void readPreset(int addr, int pcNum, int led)
// Reads preset from memory and changes the used loops and midi devices
{
  if (currentPreset == led and muteMode==0) 
  {
    //return;
  }
  if (debug) Serial.println("readPreset");

  // Get switch order value first
  intCurSwitchOrderValue = EEPROM.read(addr+9);
  // Read value as int from memory, value can be between 0-255
  int intMemory = ((EEPROM.read(addr)) & 0xFF) + (((EEPROM.read(addr+1)) << 8) & 0xFF00);
  
  // Depending on this value the order of switching the loops and midi devices is reversed
  if (intCurSwitchOrderValue==0)
  {
    // Switch Loops, then midi
    switchLoops(intMemory);
    writeMidi(addr);
  } else
  {
    // Switch midi then loops
    writeMidi(addr);
    switchLoops(intMemory);
  }
  unsigned long currentMillis = 0;
 

  // Amp switching is no longer supported
//  intRelayVal = EEPROM.read((addr)+8); // Get Reverb value
//  if (ampReverbValue!=intRelayVal)
//  {
//    digitalWrite(ampReverbRelay, LOW);
//    currentMillis = millis();
//    ampReverbValue = !ampReverbValue;
//    previousReverbMillis = currentMillis;
//  }
//  intRelayVal = EEPROM.read((addr)+9); // Get Reverb value
//  if (ampGainValue!=intRelayVal)
//  {
//    digitalWrite(ampGainRelay, LOW);
//    currentMillis = millis();
//    ampGainValue = !ampGainValue;
//    previousGainMillis = currentMillis;
//  }
  if (debug) Serial.println("Write preset LED");
  if (debug) Serial.println(led);
  intPresetLEDs = 1<<led;
  if (debug) Serial.println(intPresetLEDs);
  // Write value to presets IO
  Wire.beginTransmission(IO_ADDR_Presets);
  Wire.write(~intPresetLEDs);
  Wire.endTransmission();

  setLCDChannel();
  currentPreset = led;
  //lcd.clear();
  lcd.setCursor(0,2);
  lcd.print("                   ");
  lcd.setCursor(0,2);
  lcd.print(presetText());
  intPrevMem = intMemory;
}

void switchLoops(int memValue) 
//Switches the loops using the memValue
{
  // Determine value for each loop
  for(i=0; i<numberOfPedal; i++)
  {
    if ((memValue & 1<<(i))!=LOW){
      digitalWrite(relayPin[i], HIGH);
    } else {
      digitalWrite(relayPin[i], LOW);
    }
  }
}

void writeMidi(int addr) 
// Get midi settings from memory and write to midi out
{
  if (debug) Serial.println("read midi");
  // Read midi values
  intMidiValues[MIDIAMP] = EEPROM.read((addr)+2+MIDIAMP);
  intMidiValues[MIDIFB] = EEPROM.read((addr)+2+MIDIFB);
  intMidiValues[MIDIBS] = EEPROM.read((addr)+2+MIDIBS);
  intMidiValues[MIDIMB] = EEPROM.read((addr)+2+MIDIMB);
  // Write Midi signals (0xC0 is the midi base program change address, add the device channel and write the device preset value)
  midiProg( 0xC0 | intMidiDeviceChannels[MIDIAMP], intMidiValues[MIDIAMP]);
  midiProg( 0xC0 | intMidiDeviceChannels[MIDIFB], intMidiValues[MIDIFB]);
  midiProg( 0xC0 | intMidiDeviceChannels[MIDIBS], intMidiValues[MIDIBS]);
  midiProg( 0xC0 | intMidiDeviceChannels[MIDIMB], intMidiValues[MIDIMB]);
}

int getAddress(int channel)
{
  // Determines the memory address for the selected channel
  // Usage of the memory space is explained in the memory function 
  int localAddr = 0;
  switch(channel)
  {
    case 0: // address for preset 1
      localAddr = currentBank + 10;
      return localAddr;
      break;
    case 1: // address for preset 2
      localAddr = currentBank + 20;
      return localAddr;
      break;
    case 2:
      localAddr = currentBank + 30;
      return localAddr;
      break;
    case 3:
      localAddr = currentBank + 40;
      return localAddr;
      break;
    case 4:
      localAddr = currentBank + 50;
      return localAddr;
      break;
    case 5:
      localAddr = currentBank + 60;
      return localAddr;
      break;
    case 6:
      localAddr = currentBank + 70;
      return localAddr;
      break;
    case 7:
      localAddr = currentBank + 80;
      return localAddr;
      break;
  }
}


int getAmpSetting(int ampChannel)
{
  // Read the channel value from the analog port, not implemented
  int inputValue = analogRead(ampChannel);
  if (inputValue > 512)
  {
    if (debug) Serial.println("Channel on");
    return HIGH;
  }
  else
  {
    if (debug) Serial.println("Channel off");
    return LOW;
  }
}


void handleLoopKeyEvent (int channel)
{
  // Handles what to do when a loop buttun is pressed
  if (debug) Serial.println("presetKeyEvent");
  writeOut(channel);
  if (muteMode!=0)
  {
    mute();
  }
  // Update loop led's
  setLCDChannel();
}


void handlePresetKeyEvent(int channel)
{
  // main function for processing preset key event
  if (debug) Serial.println("presetKeyEvent");
  if (debug) Serial.println(channel);

  if (deviceMode==PRESETMODE) // Preset mode, get channel settings from memory and activate loops and swicth midi devices
  {
    // Get preset loops
    readPreset(getAddress(channel), channel+1, channel);
  }
  if (deviceMode==BANKMODE) // Bank mode, when switching to different bank. Get channel settings from memory and stop LED's blinking
  {
    currentBank=newBank;
    deviceMode=PRESETMODE;
    readPreset(getAddress(channel), channel+1, channel);
    lcd.setCursor(0,1);
    lcd.print("    Preset mode ");
  }
  if (deviceMode==MIDIMODE) // Midi mode, in this mode only button/channel 6 and 7 are handled for up and down of the current midi device value
  {
    // set new value
    if (channel==6) // up
    {
      // Handle up event
      intCurMidiValue++;
      if (intCurMidiValue > intMidiMaxValue[intCurMidi])
      {
        intCurMidiValue=0;
      }
    }
    if (channel==7) // down
    {
      // Handle up event
      intCurMidiValue--;
      if (intCurMidiValue < 0 )
      {
        intCurMidiValue=intMidiMaxValue[intCurMidi];
      }
    }
    // Update LCD to show new value and communicate new value with the midi device
    if (channel==6 || channel==7)
    {
      intMidiValues[intCurMidi] = intCurMidiValue;
      lcd.setCursor(17,2); 
      lcd.print("   ");
      lcd.setCursor(17,2); // print midi channel to lcd
      lcd.print(intCurMidiValue);
      // Send midi signal TODO
      midiProg( 0xC0 | intMidiDeviceChannels[intCurMidi], intMidiValues[intCurMidi]);
//      if (debug) Serial.println("Write midi");
//      if (debug) Serial.println(intCurMidi);
//      if (debug) Serial.println(intMidiDeviceChannels[intCurMidi]);
//      if (debug) Serial.println(intMidiValues[intCurMidi]);
    }
  }
  if (deviceMode==ORDERMODE) // Order mode, in this mode you can control the switch order, Loops versus midi devices. Only 6 and 7 are handled for reversing the order
  {
    // Change the order value
    if (channel==6) // up
    {
      // Handle up event
      intCurSwitchOrderValue++;
      if (intCurSwitchOrderValue > 1)
      {
        intCurSwitchOrderValue=0;
      }
    }
    if (channel==7) // down
    {
      // Handle up event
      intCurSwitchOrderValue--;
      if (intCurSwitchOrderValue < 0 )
      {
        intCurSwitchOrderValue=1;
      }
    }
    // Update the LCD display to show the new value
    if (channel==6 || channel==7)
    {
      intMidiValues[intCurMidi] = intCurMidiValue;
      lcd.setCursor(0,2); // print midi channel to lcd
      if (intCurSwitchOrderValue==0)
      {
        lcd.print("Loops -> Midi       ");
      }
      if (intCurSwitchOrderValue==1)
      {
        lcd.print("Midi -> Loops       ");
      }
    }
  }

  if (muteMode!=0) // When a preset is pressed in mute mode, disable mute and active preset
  {
    // TODO - Disable mute mode
    digitalWrite(muteRelay,HIGH);
    digitalWrite(muteRelay2,HIGH); 
    // digitalWrite(tuneRelay,HIGH);
    muteMode=0;
    lcd.setCursor(0,1);
    lcd.print("    Preset mode         ");
    
  }
  if (deviceMode==STOREMODE) // Store preset mode, store preset in the activated channel (channel variable)
  {
    int localAddr = 0;
    switch (channel)
    {
      case 0:
        localAddr = currentBank + 10;
        memory(localAddr,0); /* (EEPROM address, led) */
        break;
      case 1:
        localAddr = currentBank + 20;
        memory(localAddr,1); /* (EEPROM address, led) */
        break;
      case 2:
        localAddr = currentBank + 30;
        memory(localAddr,2); /* (EEPROM address, led) */
        break;
      case 3:
        localAddr = currentBank + 40;
        memory(localAddr,3); /* (EEPROM address, led) */
        break;
      case 4:
        localAddr = currentBank + 50;
        memory(localAddr,4); /* (EEPROM address, led) */
        break;
      case 5:
        localAddr = currentBank + 60;
        memory(localAddr,5); /* (EEPROM address, led) */
        break;
      case 6:
        localAddr = currentBank + 70;
        memory(localAddr,6); /* (EEPROM address, led) */
        break;
      case 7:
        localAddr = currentBank + 80;
        memory(localAddr,7); /* (EEPROM address, led) */
        break;
    }
  }
}

void changeDeviceMode(int mode)
{
// When mode is 1 we switch between looper and preset, 
// when mode is 2 we either go to store preset mode or we disable store preset mode
  if (debug) Serial.println("changeDeviceMode");
  if (mode==PROGRAMMODE && deviceMode==PRESETMODE) {deviceMode=PROGRAMMODE;}
  else if (mode==PROGRAMMODE && deviceMode==PROGRAMMODE) {deviceMode=MIDIMODE;}
  else if (mode==PROGRAMMODE && deviceMode==STOREMODE) {deviceMode=PROGRAMMODE;}
  else if (mode==PROGRAMMODE && deviceMode==MIDIMODE) {deviceMode=ORDERMODE;}
  else if (mode==PROGRAMMODE && deviceMode==ORDERMODE) {deviceMode=PRESETMODE;}
  else if (mode==STOREMODE && deviceMode==PRESETMODE) {deviceMode=STOREMODE;}
  else if (mode==STOREMODE && deviceMode==PROGRAMMODE) {deviceMode=STOREMODE;}
  else if (mode==STOREMODE && deviceMode==MIDIMODE) {deviceMode=STOREMODE;}
  else if (mode==STOREMODE && deviceMode==ORDERMODE) {deviceMode=STOREMODE;}
  else if (mode==STOREMODE && deviceMode==STOREMODE) {deviceMode=PRESETMODE;}
  if  (debug) Serial.println("New deviceMode");
  if  (debug) Serial.println(deviceMode);
  if  (debug) Serial.println(deviceMode==PRESETMODE);
  if (deviceMode==PROGRAMMODE) // set looper mode
  {
    lcd.setCursor(0,1);
    lcd.print("    Program mode");
    setLCDChannel();
  }
  if (deviceMode==MIDIMODE) // set midi mode
  {
    //Write presets 
    lcd.setCursor(0,1);
    lcd.print("     Midi mode ");
    //lcd.print(strVersion);

    lcd.setCursor(0,2);
    lcd.print("  Midi 2 select ");
    lcd.setCursor(0,3);
    lcd.print("      ");
  }
  if (deviceMode==ORDERMODE) // set order mode
  {
    //Write presets 
    lcd.setCursor(0,1);
    lcd.print("  Set switch order");
    lcd.setCursor(0,2);
    if (intCurSwitchOrderValue==0)
    {
      lcd.print("Loops -> Midi ");
    }
    if (intCurSwitchOrderValue==1)
    {
      lcd.print("Midi -> Loops       ");
    }
    lcd.setCursor(0,3);
    lcd.print("     ");
  }
  if (deviceMode==PRESETMODE) // return to preset mode
  {
    // Reset currentPreset
    int preset = currentPreset;
    currentPreset=-1;
    readPreset(getAddress(preset), 1, preset);
    lcd.setCursor(0,1);
    lcd.print("    Preset mode         ");
  }
  if (deviceMode==STOREMODE)
  {
    // lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("     Store mode           ");
    lcd.setCursor(0,2);
    lcd.print("Select preset 2store");
    // Show all Preset LED's
    intPresetLEDs = 255;
    // Write value to presets IO
    Wire.beginTransmission(IO_ADDR_Presets);
    Wire.write(~intPresetLEDs);
    Wire.endTransmission();
  }
}

void showLCDBankMode()
{
// Shows the current mode in the LCD display
    if (debug) Serial.println("currentBank = " + String(currentBank));
    // check currentBank
    lcd.setCursor(19,2);
    if (currentBank==0)
    {
      lcd.print("");
    }
    if (currentBank==100)
    {
      lcd.print("");
    }
    if (currentBank==200)
    {
      lcd.print("");
    }
}

void handleAmpBankEvent(int intButton)
{
  // Handle amp setting or bank switch event, input either 1 (reverb or down) or 2 (gain or up)
  if (deviceMode==PRESETMODE) // Preset mode => bank up or down
  {
    String strBank = "A";
    // check currentBank
    lcd.setCursor(0,1);
    if (intButton==1) // UP
    {
      if (currentBank==0) { newBank = 100; strBank="B"; }
      if (currentBank==100) { newBank = 200; strBank="C"; }
      if (currentBank==200) { newBank = 0; strBank="A"; }
    }
    // check currentBank
    if (intButton==2) // Down
    {
      if (currentBank==0) { newBank = 200; }
      if (currentBank==100) { newBank = 0; }
      if (currentBank==200) { newBank = 100; }
    }
    showLCDBankMode();
    // Update LCD
    lcd.setCursor(0,1);
    String strText = "Select bank " + strBank + " preset";
    lcd.print(strText);
    // Show blinking pedal LED's
//    for(i=0; i<numberOfPedal; i++)
//    {
//      digitalWrite(ledPin[i], HIGH);
//    }
    // Set BANKMODE
    deviceMode=BANKMODE;
  }
  if (deviceMode==PROGRAMMODE) // In program mode switch amp reverb setting (no longer used)
  {
    unsigned long currentMillis = millis();
    if (intButton==1)
    {
      digitalWrite(ampReverbRelay, LOW);
      ampReverbValue = !ampReverbValue;
      previousReverbMillis = currentMillis;
    }
    if (intButton==2)
    {
      digitalWrite(ampGainRelay, LOW);
      ampGainValue = !ampGainValue;
      previousGainMillis = currentMillis;
    }
    // Update LCD
    setLCDAmpSettings();
  }
  if (deviceMode==MIDIMODE) // Show midi mode settings in LCD display cycle through available midi devices and show their settings
  {
    if (intButton==1)
    {
      String strMidi = "                    ";
      intCurMidi = intCurMidi + 1;
      if (intCurMidi > 3) 
      {
        intCurMidi = 0;
      } 
      intCurMidiValue = intMidiValues[intCurMidi];
      lcd.setCursor(0,2); // print midi channel to lcd
      lcd.print(strMidiDevices[intCurMidi]);
      lcd.setCursor(17,2); 
      lcd.print("   ");
      lcd.setCursor(17,2); // print midi channel to lcd
      lcd.print(intCurMidiValue);
      lcd.setCursor(0,3);
      lcd.print("  _up(7)    (8)down_");
    }
  }
}

/******************************************************/
//take care of some special events
void keypadEvent(KeypadEvent key){
  // Main function for figuring out which button was pressed by the guitarist
  if (debug) Serial.println("key event");
  if (debug) Serial.println(key);
  switch (keypad.getState())
  {
    case PRESSED:  // In the down event most time critical events are passed, being mute, the preset buttons, loop buttons and the bank buttons
      startMillis = millis();
      if (debug) Serial.println("key pressed event");
      if (debug) Serial.println(key);
      if (key=='j') // 'j' is mute mode
      {
        // Bypass all pedals
        mute();
      }
      else 
      {
        
        switch (key)
        {
           case 'a': // 'a' to 'h' represent a preset button pressed
            handlePresetKeyEvent(0);
            break;
           case 'b':
            handlePresetKeyEvent(1); 
            break;
           case 'c':
            handlePresetKeyEvent(2);
            break;
           case 'd':
            handlePresetKeyEvent(3);
            break;
           case 'e':
            handlePresetKeyEvent(4);
            break;
           case 'f':
            handlePresetKeyEvent(5);
            break;
           case 'g':
            handlePresetKeyEvent(6);
            break;
           case 'h':
            handlePresetKeyEvent(7);
            break;
           case 'm': // 'm' to 'X' represent a loop key pressed
            handleLoopKeyEvent(0); 
            break;
           case 'n':
            handleLoopKeyEvent(1); 
            break;
           case 'o':
            handleLoopKeyEvent(2); 
            break;
           case 'p':
            handleLoopKeyEvent(3); 
            break;
           case 'q':
            handleLoopKeyEvent(4); 
            break;
           case 'r':
            handleLoopKeyEvent(5); 
            break;
           case 's':
            handleLoopKeyEvent(6); 
            break;
           case 't':
            handleLoopKeyEvent(7); 
            break;
            case 'u':
            handleLoopKeyEvent(8); 
            break;
            case 'v':
            handleLoopKeyEvent(9); 
            break;
            case 'w':
            handleLoopKeyEvent(10); 
            break;
            case 'x':
            handleLoopKeyEvent(11); 
            break;
           
           
           // TODO - Insert handler for moving through the midi settings
           case 'k': // depending on mode either amp Reverb switch or bank down, the midi handling in program mode is also handled
            handleAmpBankEvent(1);
            break;
           case 'l': // depending on mode either amp Channel switch or bank up
            handleAmpBankEvent(2);
            break;
        }
      }
    break;
    case RELEASED: // processes the buttons in the event they are released
      if (debug) Serial.println("key released event");
      if (debug) Serial.println(key);
      // saveState check savestate
      switch (key)
      {
         case 'i': // 'i' represents the mode button
          if (holdProcessed == 0)
          {
            changeDeviceMode(1);
          }
          else holdProcessed = 0;
          break;
//         case 'k': // depending on mode either amp Reverb switch or bank down, the midi handling in program mode is also handled
//          memoryDump();
//          break;
      }
    break;
    case HOLD: // processes the buttons in the event they are hold a longer time (3 seconds)
      if (debug) Serial.println("key hold event");
      if (debug) Serial.println(key);
      switch (key)
      {
        /****************************** STORE PRESET MODE */
         case 'i': // 'i' represents the mode button, when hold it triggers the store preset mode
          holdProcessed = 1;
          changeDeviceMode(STOREMODE);
          break;
         case 'j': // Enable Mute + mode (blinking LEDs)
          if (muteMode!=0)
          {
            muteMode = 2;
            // store value in Prev variable
            intPresetLEDsPrev = intPresetLEDs;
            // Show all Preset LED's
            intPresetLEDs = 255;
            // Write value to presets IO
            Wire.beginTransmission(IO_ADDR_Presets);
            Wire.write(~intPresetLEDs);
            Wire.endTransmission();
            intLoopLEDsPrev = intLoopLEDs;
            intLoopLEDs = 255;
            // Write value to presets IO
            Wire.beginTransmission(IO_ADDR_Loops);
            Wire.write(~intLoopLEDs);
            Wire.endTransmission();
            lcd.setCursor(0,1);
            lcd.print("  led blink test :) ");
          }
          break;
      }
    break;
  }
}

void showTimer()
{
  // Shows duration from keyPressed event till you insert the showTimer call, used when performace testing during development of the hardware and software
  unsigned long currentMillis = millis();
  unsigned long duration = currentMillis - startMillis; 
  // Display duration
  lcd.setCursor(0,3);
  lcd.print("                    ");
  lcd.setCursor(0,3);
  lcd.print(duration);
  lcd.setCursor(18,3);
  lcd.print("xx");
}

/******************************************************/

void loop()
{
  // Trigger keypad events
  char key = keypad.getKey(); // When one of the buttons is pressed it will trigger the keypadEvent() function
  
  // check for store preset mode, if found blink leds
  unsigned long currentMillis = millis();
  if (deviceMode==STOREMODE and currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    // reverse the preset LEDs
    intPresetLEDs = ~intPresetLEDs;
    // Write value to presets IO
    Wire.beginTransmission(IO_ADDR_Presets);
    Wire.write(~intPresetLEDs);
    Wire.endTransmission();
  }

  if (muteMode==2 and currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    // reverse the preset LEDs
    intPresetLEDs = ~intPresetLEDs;
    // Write value to presets IO
    Wire.beginTransmission(IO_ADDR_Presets);
    Wire.write(~intPresetLEDs);
    Wire.endTransmission();
    intLoopLEDs = ~intLoopLEDs;
    byte LoByte = (intLoopLEDs & 0x00FF);
    byte HiByte = (((intLoopLEDs) >>8) & 0x00FF);
  
    // Write value to presets IO
    Wire.beginTransmission(IO_ADDR_Loops);
    Wire.write(~LoByte);
    Wire.endTransmission();
    Wire.beginTransmission(IO_ADDR_Loops2);
    Wire.write(~HiByte);
    Wire.endTransmission();
  
  }

  if (previousReverbMillis!=0 and currentMillis - previousReverbMillis >= 50 )
  {
    digitalWrite(ampReverbRelay, HIGH);
    previousReverbMillis = 0;
  }
  if (previousGainMillis!=0 and currentMillis - previousGainMillis >= 50 )
  {
    digitalWrite(ampGainRelay, HIGH);
    previousGainMillis = 0;
  }
}
