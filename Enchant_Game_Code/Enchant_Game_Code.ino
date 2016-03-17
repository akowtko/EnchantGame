/*
Code for the entirety of the Enchant game. When declaring the Player
object, use either ASSASSIN or DEFENDER depending on which role the
player has for the Arduino connected to this code.

The difference between the two roles is the assassin transmits its 
packet first while the defender is in listening mode, then once it
receives confirmation that the packet was successfully sent, the
assassin switches to listening mode and the defender begins
transmitting, etc.
 * 
 - CONNECTIONS: nRF24L01 Modules See:
 http://arduino-info.wikispaces.com/Nrf24L01-2.4GHz-HowTo
   1 - GND
   2 - VCC 3.3V !!! NOT 5V
   3 - CE to Arduino pin 9
   4 - CSN to Arduino pin 10
   5 - SCK to Arduino pin 13
   6 - MOSI to Arduino pin 11
   7 - MISO to Arduino pin 12
   8 - UNUSED (but plugged in anyways)
*/

/*-----( Import needed libraries )-----*/
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Player.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include "Adafruit_DRV2605.h"

/*-----( Declare Constants and Pin Numbers )-----*/
#define CE_PIN   9
#define CSN_PIN 10
#define thumb A0
#define pointer A1
#define ring A2
#define NEOPIXELPIN 6
#define CENTERLEFTLED 14    //The number for the left led at the top of the circle closest to the fingers
#define CENTERRIGHTLED 13
#define NUMPIXELS 16
#define DEFENDER 100
#define ASSASSIN 200
#define HEALTH 6
// NOTE: the "LL" at the end of the constant is "LongLong" type
const uint64_t pipe = 0xE8E8F0F0E1LL; // Define the transmit pipe


/*-----( Declare objects )-----*/
Adafruit_DRV2605 drv; //Create haptic controller
RF24 radio(CE_PIN, CSN_PIN); // Create a Radio
Player Player(DEFENDER, HEALTH);         //Create a defender
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, NEOPIXELPIN); //create neopixel ring

/*-----( Declare Variables )-----*/
boolean debug = true;
float recPacket[4] = {0};            // [userID, isAlive, defValue, atkValue] 
float sendPacket[4] = {0};           // [userID, isAlive, defValue, atkValue] 
int bend = 620;             //Any value above this is a bend
int maxBend = 700;          //Any value above this is the max bend possible
int hand[3];                //array to store the three flex sensor values
int firstLoop = 1;
int ledToLightUp = 0;       //Keeps track of which led in the ring to light up next
float chargeFraction = 1.0/16; //The fraction of leds lit up in the ring
uint32_t off = pixels.Color(0, 0, 0); //The RGB values for the led to be off
uint32_t on = pixels.Color(255, 255, 255);
int healthBar[HEALTH] = {1,1,1,1,1,1};
int healthBarPins[HEALTH] = {A3,7, 5, 4, 3, 2};
int lenHealthBar = HEALTH;
bool radioNumber;
byte addresses[][6] = {"1Node","2Node"};              // Radio pipe addresses for the 2 nodes to communicate.
typedef enum { role_ping_out = 1, role_pong_back } role_e;                 // The various roles supported by this sketch
const char* role_friendly_name[] = { "invalid", "Ping out", "Pong back"};  // The debug-friendly names of those roles
role_e role;
int whileCounter = 0;
int cannotConnect = 0;
int connectThresh = 500;
int wasCharging = 1;

void setup()
{
  Serial.begin(9600);

  //Setting up neopixel ring
  pixels.begin();
  pixels.setBrightness(220);

  //Set up vibration motor
  drv.begin();
  drv.selectLibrary(1);
  drv.setMode(DRV2605_MODE_INTTRIG); 
  
  //Setting up flex sensors
  pinMode(thumb, INPUT);
  pinMode(pointer, INPUT);
  pinMode(ring, INPUT);

  //Variables are set differently if the user is the Defender
  //or Attacker
  if(Player._userID == DEFENDER){
    radioNumber = 1;
    role = role_pong_back;
    if(debug) Serial.print("Player ID: ");
    if(debug) Serial.println(DEFENDER);
    if(debug) Serial.print("Assassin ID: ");
    if(debug) Serial.println(ASSASSIN);
    Player.setAssassinID(ASSASSIN);
    //Set up the health bar, which only the defender has
    pinModeHealthBar();
    updateHealthBar();
  }
  //If the player is the attacker...
  else{
    radioNumber = 0;
    role = role_ping_out;
    if(debug) Serial.print("Player ID: ");
    if(debug) Serial.println(ASSASSIN);
    if(debug) Serial.print("Target ID: ");
    if(debug) Serial.println(DEFENDER);
    Player.setTargetID(DEFENDER);
  }

  //Setup and configure radio for the Defender
  radio.begin();
  if(radioNumber){
    radio.openWritingPipe(addresses[1]);        // Both radios listen on the same pipes by default, but opposite addresses
    radio.openReadingPipe(1,addresses[0]);      // Open a reading pipe on address 0, pipe 1
  }
  //Setup and configure radio for the Attacker
  else{
    radio.openWritingPipe(addresses[0]);
    radio.openReadingPipe(1,addresses[1]);
  }

  radio.startListening(); //Have the radio start listening for other players.
  
  delay(1000);
  if(debug) Serial.println("Beginning Game...");
  gameStartBuzz();
  delay(2000);
}


void loop()
{ 
  //Assassin Loop
  if(role == role_ping_out){
    assassinLoop();
  }

  //Defender Loop
  else if(role == role_pong_back){
    defenderLoop();
  }
}

//*****************
//*assassinLoop()
//*****************
//The entire gameplay code loop for the assassin
void assassinLoop(){
  //Put the assassin into transmit packet mode.
  radio.stopListening();
  cannotConnect = 0;

  //Reads the glove's current state
  readHand();
  Player.checkGesture(hand);

  //Checks if assassin is charging, and lights up
  //NeoPixel ring for charging sequence
  if(Player._charging){
    gestureCharging();
    wasCharging = 1;
    Serial.print("Current Atk Value: ");
    Serial.println(Player._chargingAtkValue);
  }
  //if the assassin is no longer charging, but was in the previous
  //turn, then send the attack and clear previous values
  else if(wasCharging == 1){
    Player._atkValue = Player._chargingAtkValue;
    sentAttackOrDefense(); 
    Serial.print("Attack Value: ");
    Serial.println(Player._atkValue);

  }

  //Prepares the packet of info for the radio transceiver to send
  //to the defender
  prepPacket();

  //Try to write packet until received
  while(!radio.write(sendPacket, sizeof(sendPacket))){
    //If it hasn't been received for a long time, the radio may not be available
    if(cannotConnect > connectThresh){
      //indicates to the game that the next loop is the "first" loop
      //so it will break from an infinite loop and try searching for
      //the other player again
      firstLoop = 1;
      break;
    }
    else{
      cannotConnect++;
    }
  }
  //Breaking from the while loop means we did connect with the other player, so
  //set cannotConnect to 0
  cannotConnect = 0;
  //print the sent packet to the serial monitor
  printPacket();

  //Allows programmer to add "player Near" buzz
  if(firstLoop == 1){
    //playerNearBuzz();
    firstLoop = 0;
  }

  //Since the assassin has now sent the packet, it changes
  //to listen mode to wait for the defender's response
  radio.startListening();
      
  while(!radio.read( recPacket, sizeof(recPacket) )){
    //If it hasn't been received for a long time, the radio may not be available
    if(cannotConnect > connectThresh){
      firstLoop = 1;
      break;
    }
    else{
      cannotConnect++;
    }
  }

  cannotConnect = 0;
  printRecPacket();

  //Similar to above sequence, but double checks the assassin's
  //gestures after receiving the defender's gestures to ensure only
  //the most up to date values are compared when checking if the attack
  //was successful
  if(Player._charging){
  }
  else if(wasCharging == 1){
    wasCharging = 0;
    sentAttackOrDefense(); 

    //***Check to see if the attack was successful
    int successfulAtk = Player.attackSuccessful(recPacket[2]);
    //Since the attack was sent, clear out variables
    Player._charging = false;
    Player._chargingTime = 0;
    Player._chargingTimeMax = 0;
    Player._chargingAtkValue = 0;
    Player._defValue = 0;
    //Do haptic feedback if it was successful
    if(successfulAtk){
      if(debug) Serial.println("Attack successful");
      hit();
    }
    else{
      if(debug) Serial.println("Attack UNsuccessful");
      miss();
    }
  }
  //If the assassin isn't sending an attack or currently charging,
  //don't do anything
  else{
    Player._atkValue = 0;
    doNothingGesture();
  }

  Serial.println();
  //Stops listening in preparation of restarting the loop
  //where the assassin writes their packet first, then listens
  radio.stopListening();
} 


//*****************
//*defenderLoop()
//*****************
//The entire gameplay code loop for the defender
void defenderLoop(){
  //defender starts by listening for the assassin's sent packet
  radio.startListening();
  cannotConnect = 0;

  if(firstLoop == 1){
    firstLoop = 0;
  }
  

  //Try to read packet until defender successfully receives it
  while(!radio.read(recPacket, sizeof(recPacket))){
    //If it hasn't been received for a long time, the radio may not be available
    //in which case break from the infinite loop and restart "defenderLoop()"
    if(cannotConnect > connectThresh){
      firstLoop = 1;
      break;
    }
    else{
      cannotConnect++;
    }
  }
  cannotConnect = 0;

  //Print the received packet
  printRecPacket();
  //Stop listening to go into transmit mode to send packet
  radio.stopListening();

  //gesture processing
  readHand();
  Player.checkGesture(hand);
  //prepares packet to send via transceiver
  prepPacket(); 
  radio.write(sendPacket, sizeof(sendPacket));    

  //Checks if the assassin's attack value is greater than 0, in which case
  //an attack was sent
  if(recPacket[3] > 0){
    wasCharging = 0;
    sentAttackOrDefense();
    
    //Check to see if the attack was successful
    int successfulAtk = Player.attackSuccessful(recPacket[3]);
    Player._charging = false;

    if(successfulAtk){
      if(debug) Serial.println("You've been hit.");
      hit();
      updateHealth();
      updateHealthBar();
    }
    else{
      if(debug) Serial.println("The attacker missed.");
      miss();
    }      
  }
  
  else if(Player._charging){
    gestureCharging();
    wasCharging = 1;
  }

  //If the defender isn't currently defending but was in the previous
  //turn and the assassin sent an attack, also check whether the attack
  //was successful
  else if(wasCharging == 1 && recPacket[3] > 0){
    wasCharging = 0;
    sentAttackOrDefense();
    
    //Check to see if the attack was successful
    int successfulAtk = Player.attackSuccessful(recPacket[3]);
    Player._charging = false;

    if(successfulAtk){
      if(debug) Serial.println("You've been hit.");
      hit();
      updateHealth();
      updateHealthBar();
    }
    else{
      if(debug) Serial.println("The attacker missed.");
      miss();
    }    
  }
  
  //If we're no longer charging, e.g. we just sent an attack, reset variables
  else{
    doNothingGesture();
    Player._charging = false;
    Player._chargingTime = 0;
    Player._chargingTimeMax = 0;
    Player._chargingAtkValue = 0;
    Player._atkValue = 0;
    Player._defValue = 0;
  }

  printPacket();
  Serial.println();
  radio.startListening();
  
  if(!radio.available()){
    //if(debug) Serial.println("Player not nearby");
  } 
}


//*****************
//*readHand()
//*****************
//Reads the flex sensor values for the thumb, pointer, and ring fingers
//and saves the value in the "hand" array.
void readHand(){
  hand[0] = analogRead(thumb);
  hand[1] = analogRead(pointer);
  hand[2] = analogRead(ring);
  checkBends();
  if(debug){
    Serial.print("Hand Gesture: [");
    Serial.print(hand[0]);
    Serial.print(", ");
    Serial.print(hand[1]);
    Serial.print(", ");
    Serial.print(hand[2]);
    Serial.println("]");
  }
}

//*****************
//*printRecPacket()
//*****************
//Prints the packet received by the transceiver from the other player
void printRecPacket(){
  Serial.print("Other UserID = ");
  Serial.print(recPacket[0]);
  Serial.print(", isAlive = ");      
  Serial.print(recPacket[1]);
  Serial.print(", Defence Value = ");
  Serial.print(recPacket[2]);
  Serial.print(", Attack Value = ");
  Serial.println(recPacket[3]);
}


//*****************
//*printRecPacket()
//*****************
//Lights up the neoPixel ring to indicate what percent the gesture has been
//charged, and gives a haptic buzz to indiciate the player is charging
void gestureCharging(){
  //Haptic buzz to indicate charging
  chargingBuzz();
  if(debug) Serial.println("Gesture charging");
  //determines how much the player has charged the gesture by checking the current charge time
  //compared to the maximum, a value stored in the gesture's arrawy
  float currentChargeFraction = Player._chargingTime / Player._chargingTimeMax;
  if(debug) Serial.print("Current Charge Fraction: ");
  if(debug) Serial.println(currentChargeFraction);
  //Checks if the charge is great enough to light up another LED on the NeoPixel ring
  if(currentChargeFraction >= chargeFraction){
    
    if(debug) Serial.println("Trigger next LED on ring");
    ledRingCharge();
    //This ensures after being fully charged we won't try to relight up the ring
    if(chargeFraction == 1){
      chargeFraction = 1.5;
    }
    //If the gesture isn't fully charged, increment the charge fraction by 1/16
    else{
      chargeFraction = chargeFraction + 1.0/16;
    }
    if(debug) Serial.print("Next charge fraction: ");
    if(debug) Serial.println(chargeFraction);
  }
}


//*****************
//*sentAttackOrDefense()
//*****************
//Resets variables after sending gesture, and triggers NeoPixel "gesture sent" lights
void sentAttackOrDefense(){
  chargeFraction = 0;  
  if(debug) Serial.println("Turning LEDs Off");
  turnLEDsOff();
  ledRingGestureSent(); //attack sent light effects
  ledToLightUp = 0;  
}


//*****************
//*doNothingGesture()
//*****************
//Turns all the LEDs off and resets charge value
void doNothingGesture(){
  chargeFraction = 0;  
  if(debug) Serial.println("Turning LEDs Off");
  turnLEDsOff();
  ledToLightUp = 0;  
}

//*****************
//*prepPacket()
//*****************
//Prepares packet for the transceiver to send to the other player
void prepPacket(){
  sendPacket[0] = Player._userID;
  sendPacket[1] = Player._alive;
  sendPacket[2] = Player._defValue;
  sendPacket[3] = Player._atkValue;
}

//*****************
//*printPacket()
//*****************
//Prints the packet sent by the transceiver to the other player
void printPacket(){
  Serial.print("Sent UserID: ");
  Serial.print(sendPacket[0]);
  Serial.print(", isAlive: ");
  Serial.print(sendPacket[1]);
  Serial.print(", Defence Value: ");
  Serial.print(sendPacket[2]);
  Serial.print(", Attack Value: ");
  Serial.println(sendPacket[3]);
}

//*****************
//*ledRingCharge()
//*****************
//Lights up the next LED in the neoPixel ring
void ledRingCharge(){
  pixels.setPixelColor(ledToLightUp, on);
  pixels.show();

  //increments the ledToLightUp counter
  if(ledToLightUp == 16){
    ledToLightUp = 0;
  }
  else{
    ledToLightUp++;
  }
}

//*****************
//*ledRingGestureSent()
//*****************
//Gesture sent lighting effect function, which currently doesn't work
void ledRingGestureSent(){
  int lightDelay = 100;
  int leftLED = CENTERLEFTLED;
  int rightLED = CENTERRIGHTLED;
  int decrementLeft = incrementLeftLED(incrementLeftLED(leftLED));
  int decrementRight = incrementRightLED(incrementRightLED(rightLED));

  for(int i=1; i<9; i++){
      pixels.setPixelColor(leftLED, on);
      pixels.setPixelColor(rightLED, on);
      delay(lightDelay/2);
      pixels.setPixelColor(decrementLeft, off);
      pixels.setPixelColor(decrementRight, off);
      
      leftLED = incrementLeftLED(leftLED);
      rightLED = incrementRightLED(rightLED);
      decrementLeft = incrementLeftLED(incrementLeftLED(leftLED));
      decrementRight = incrementRightLED(incrementRightLED(rightLED));
      delay(lightDelay);
  }
}

//*****************
//*incrementLeftLED()
//*****************
//Helper function for ledRingGestureSent() incrementing the variable "leftLED"
int incrementLeftLED(int leftLED){
  if(leftLED == 15){
    leftLED = 0;
  }
  else{
    leftLED ++;
  }
}

//*****************
//*incrementRightLED()
//*****************
//Helper function for ledRingGestureSent() incrementing the variable "rightLED"
int incrementRightLED(int rightLED){
  if(rightLED == 0){
    rightLED = 15;
  }
  else{
    rightLED --;
  }
}

//*****************
//*turnLEDsOff()
//*****************
//Turns all the LEDs in the NeoPixel ring off
void turnLEDsOff(){
  for(int i=0; i<NUMPIXELS; i++){
    pixels.setPixelColor(i, off);
    pixels.show();
  }
}

//*****************
//*pinModeHealthBar()
//*****************
//Sets each of the pins attached to the health bar to OUTPUT mode 
void pinModeHealthBar(){
  pinMode(A3, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(7, OUTPUT);
}

//*****************
//*updateHealthBar()
//*****************
//Lights up the health bar based on the current health, represented in an array
void updateHealthBar(){
  for(int i=0; i<lenHealthBar; i++){
    //if the current index in healthBar is 1, meaning the user still has that
    //health point, keep the green LED lit up
    if(healthBar[i] == 1){
      digitalWrite(healthBarPins[i],HIGH);
    }
    //If the index is 0, meaning the user no longer has that health point,
    //flash that led before turning it off
    else{
      digitalWrite(healthBarPins[i],HIGH);
      delay(50);
      digitalWrite(healthBarPins[i],LOW);
      digitalWrite(healthBarPins[i],HIGH);
      delay(50);
      digitalWrite(healthBarPins[i],LOW);
    }
  }
}

//*****************
//*updateHealth()
//*****************
//updates the healthBar array based on the player's current health
void updateHealth(){
  for(int i=0; i<Player.getHealth(); i++){
    healthBar[i] = 1;
  }
  for(int i=Player.getHealth(); i<lenHealthBar; i++){
    healthBar[i] = 0;
  }
}

//*****************
//*checkBends()
//*****************
//Checks whether the raw flex sensor value matches our threshold for a bend
void checkBends(){
  for(int i=0; i<3; i++){
    if(hand[i] < bend){
      hand[i] = 0;
    }
    else{
      hand[i] = 1;
    }
  }
}


//Vibration motor pattern to indicate the player is charging a gesture.
void chargingBuzz(){
  drv.setWaveform(0, 108);
  drv.setWaveform(1, 109);
  drv.setWaveform(2, 0);  // end of waveforms
  drv.go();
}

//Vibration motor pattern to indicate the beginning of gameplay.
void gameStartBuzz(){
  drv.setWaveform(0, 92);
  drv.setWaveform(1, 44); 
  drv.setWaveform(2, 44);
  drv.setWaveform(3, 45);
  drv.setWaveform(4, 46);
  drv.setWaveform(5, 0);
  drv.go();
}

//Vibration motor pattern to indicate the attack missed.
void miss(){
  drv.setWaveform(0, 13);
  drv.setWaveform(1, 0);
  drv.go();
}

//Vibration motor pattern to indicate the attack hit.
void hit(){
  drv.setWaveform(0, 15);
  drv.setWaveform(1, 27);
  drv.setWaveform(2, 0);
  drv.go();
}

