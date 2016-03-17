/*
 * Player.h - Library for player
 */
#ifndef PLAYER_H
#define PLAYER_H

#include "Arduino.h"

class Player
{
  public:
    Player(int userID, int health);
    void setTargetID(int targetID);
    void setAssassinID(int assaassinID);
    int getHealth();            
    boolean targetNearby(int userID);
    boolean assassinNearby(int userID);
    boolean attackSuccessful(int Value);     
    void decreaseHealth(int value);
    void increaseHealth(int value);
    void checkGesture(int hand[3]);
    void copyArray(int array[], int copy[], int len);
    boolean compareArray(int array1[], int array2[]);
    boolean _alive;  
    float _atkValue = 0;
    float _defValue = 0;  
    int _userID;
    bool _charging;
    float _chargingTime = 0; //How many loops have we been charging for
    float _chargingTimeMax;
    float _chargingAtkValue = 0; 
     
  private:
    int _targetID;
    int _assassinID;
    int _health;
    boolean _healMode;
    int _spiderman[6] = {0,0,1,3,20,1};
    int _ironman[6] = {0,1,1,4,35,1};
    int _hulk[6] = {1,1,1,5,45,1};
    int _defence[6] = {1,0,0,5,48,0};
    int _none[6] = {0,0,0,0,0,0};
    int _gesture[6]; 
};

#endif

