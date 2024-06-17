#include <MIDIUSB.h>

//Digital Pins
const int buttonPins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10};
const int s0Pin = 16;
const int s1Pin = 14;
const int s2Pin = 15;

//Analog Pins
const int rot1Pin = A0;
const int togglePin = A1;
const int joyButtonPin = A2;
const int muxPin = A3;

// MUX Inputs
const int muxChannels[] = {0, 1, 2, 3, 4, 5, 6, 7};

//Number of Buttons
const int numButtons = sizeof(buttonPins) / sizeof(buttonPins[0]);   

//Drum Pad Lowest Note
const int baseNote = 36; // MIDI Number for C1

//Debouncing, Button States
bool buttonStates[numButtons];
bool lastButtonStates[numButtons];
unsigned long lastDebounceTime[numButtons];
unsigned long debounceDelay = 50; // Delay in Milliseconds

//Analog State Memory
int lastRot1Value = -1;
int lastMuxValues[8];
bool lastToggleState = false;
int lastJoyButtonState = -1;

//Analog Changing Threshold
const int analogThreshold = 2;

//Potentiometer Smoothening (Averaging) for rot1Pin
const int numReadings = 10;
int rot1Readings[numReadings];      
int rot1Index = 0;                  
int rot1Total = 0;                 
int rot1Average = 0;                 

//Potentiometer Smoothening (Averaging) for MUX Inputs
int muxReadings[8][numReadings];
int muxIndexes[8];
int muxTotals[8];
int muxAverages[8];

void setup() {
  //Initialization
  //Serial and set Baud Rate
  Serial.begin(115200);
  while (!Serial); //Wait Until Ready
  //Serial.println("Arduino MIDI Controller Initialized"); //Debugging

  //Button Matrix
  for (int i = 0; i < numButtons; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    buttonStates[i] = HIGH;
    lastButtonStates[i] = HIGH;
    lastDebounceTime[i] = 0;
  }

  //Direct Analog Inputs
  pinMode(rot1Pin, INPUT);
  pinMode(togglePin, INPUT_PULLUP);
  pinMode(joyButtonPin, INPUT_PULLUP);

  //MUX Control Pins
  pinMode(s0Pin, OUTPUT);
  pinMode(s1Pin, OUTPUT);
  pinMode(s2Pin, OUTPUT);

  //Last Mux Values
  for (int i = 0; i < 8; i++) {
    lastMuxValues[i] = -1;
    muxIndexes[i] = 0;
    muxTotals[i] = 0;
    muxAverages[i] = 0;
    for (int j = 0; j < numReadings; j++) {
      muxReadings[i][j] = 0;
    }
  }

  //Array for Pot Smoothing for rot1Pin
  for (int i = 0; i < numReadings; i++) {
    rot1Readings[i] = 0;
  }

  //Toggle Switch State
  lastToggleState = digitalRead(togglePin);
}

void loop() {
  //Read Buttons, Send MIDI Messages
  for (int i = 0; i < numButtons; i++) {
    int reading = digitalRead(buttonPins[i]);
    if (reading != lastButtonStates[i]) {
      lastDebounceTime[i] = millis();
    }
    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (reading != buttonStates[i]) {
        buttonStates[i] = reading;
        int noteNumber = baseNote + (numButtons - 1 - i);
        if (buttonStates[i] == LOW) {
          midiEventPacket_t noteOn = {0x09, 0x90 | 0, noteNumber, 127};
          MidiUSB.sendMIDI(noteOn);
          //Serial.print("Note On: ");  //Debug
          //Serial.println(noteNumber); //Debug
        } else {
          midiEventPacket_t noteOff = {0x08, 0x80 | 0, noteNumber, 0};
          MidiUSB.sendMIDI(noteOff);
          //Serial.print("Note Off: "); //Debug
          //Serial.println(noteNumber); //Debug
        }
      }
    }
    lastButtonStates[i] = reading;
  }

  //Read Rotary Potentiometer, Send MIDI CC
  int rot1Value = analogSmooth(rot1Pin, rot1Readings, rot1Total, rot1Index, rot1Average); //Smoothened Value
  int mappedRot1Value = map(rot1Value, 0, 1023, 0, 127); // Map to 0-127 range
  if (abs(mappedRot1Value - lastRot1Value) > analogThreshold) {
    midiEventPacket_t controlChange = {0x0B, 0xB0 | 0, 1, mappedRot1Value};
    MidiUSB.sendMIDI(controlChange);
    //Serial.print("Control Change: ");  //Debug
    //Serial.println(mappedRot1Value);   //Debug
    lastRot1Value = mappedRot1Value;
  }

  //Read Toggle, Send MIDI Message
  bool currentToggleState = digitalRead(togglePin);
  if (currentToggleState != lastToggleState) {
    midiEventPacket_t toggleNote;
    if (currentToggleState == HIGH) {
      toggleNote = {0x09, 0x90 | 0, baseNote + 9, 127};
      //Serial.println("Toggle Switch On");   //Debug
    } else {
      toggleNote = {0x08, 0x80 | 0, baseNote + 9, 0};
      //Serial.println("Toggle Switch Off");  //Debug
    }
    MidiUSB.sendMIDI(toggleNote);
    lastToggleState = currentToggleState;
  }

  //Read Joystick Button, Send MIDI Message
  int joyButtonState = digitalRead(joyButtonPin);
  if (joyButtonState != lastJoyButtonState) {
    if (joyButtonState == LOW) {
      midiEventPacket_t joyNoteOn = {0x09, 0x90 | 0, baseNote + 10, 127};
      MidiUSB.sendMIDI(joyNoteOn);
      //Serial.println("Joystick Button On");  //Debug
    } else {
      midiEventPacket_t joyNoteOff = {0x08, 0x80 | 0, baseNote + 10, 0};
      MidiUSB.sendMIDI(joyNoteOff);
      //Serial.println("Joystick Button Off"); //Debug
    }
    lastJoyButtonState = joyButtonState;
  }

  //Read MUX Inputs, Send MIDI CC
  for (int i = 0; i < 8; i++) {
    setMuxChannel(i);
    int muxValue = analogSmooth(muxPin, muxReadings[i], muxTotals[i], muxIndexes[i], muxAverages[i]); //Smoothened Value
    int mappedMuxValue = map(muxValue, 0, 1023, 0, 127); // Map to 0-127 range
    if (abs(mappedMuxValue - lastMuxValues[i]) > analogThreshold) {
      int controlChangeNumber = i + 2; //Ensure Unique CC Numbers
      midiEventPacket_t muxControlChange = {0x0B, 0xB0 | 0, controlChangeNumber, mappedMuxValue};
      MidiUSB.sendMIDI(muxControlChange);
      //Serial.print("MUX Control Change Channel ");  //Debug
      //Serial.print(i);
      //Serial.print(": ");
      //Serial.println(mappedMuxValue);
      lastMuxValues[i] = mappedMuxValue;
    }
  }

  //Allow for Other USB Tasks
  MidiUSB.flush();
  delay(10); //Prevent Overwhelming MIDI buffer
}

//Function, Set MUX channel
void setMuxChannel(int channel) {
  digitalWrite(s0Pin, bitRead(channel, 0));
  digitalWrite(s1Pin, bitRead(channel, 1));
  digitalWrite(s2Pin, bitRead(channel, 2));
}

//Function, Smooth Analog Readings
int analogSmooth(int pin, int readings[], int &total, int &index, int &average) {
  total = total - readings[index];
  readings[index] = analogRead(pin);
  total = total + readings[index];
  index = (index + 1) % numReadings;
  average = total / numReadings;
  return average;
}
