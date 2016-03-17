/*
 * Player.cpp
 * UserID = 100 means Defender
 * UserID = 200 means Assassin/Attacker
 */

 #include "Arduino.h"
 #include "Player.h"

 Player::Player(int userID, int health){
  _userID = userID;
  _alive = true;
  _health = health;
 }

 void Player::setTargetID(int targetID){
  _targetID = targetID;
 }

 void Player::setAssassinID(int assassinID){
  _assassinID = assassinID;
 }

 int Player::getHealth(){
  return _health;
 }

 boolean Player::targetNearby(int userID){
  if(userID == _targetID){
    return true;
  }
  else{
    return false;
  }
 }

 boolean Player::assassinNearby(int userID){
  if(userID == _assassinID){
    return true;
  }
  else{
    return false;
  }
 }


//If Player = target, Value = assassin's attack
//If Player = assassin, Value = target's defence
boolean Player::attackSuccessful(int Value){
  //If this is the attacker, we only care about if it hit
  if(_atkValue != 0){
    //If target sustained damage, it's a hit
    if(_atkValue - Value > 0){
      return true;
    }
    else{
      return false;
    }
  }
  //If this is the defender we need to change the heatlh
  else{
    //If it's a hit
    if(Value - _defValue > 0){
      decreaseHealth(Value - _defValue);
      if(_health <= 0){
        _alive == false;
      }
      return true;
    }
    else{
      return false;
    }
  }
  _charging = false;
  _chargingTime = 0;
  _chargingTimeMax = 0;
  _chargingAtkValue = 0;
  _atkValue = 0;
  _defValue = 0;

  

  
}

void Player::decreaseHealth(int value){
  _health = _health - value;
}

void Player::increaseHealth(int value){
  _health = _health + value;
}

void Player::checkGesture(int hand[3]){
  if(compareArray(hand, _hulk) && _userID == 200){
    copyArray(_hulk, _gesture, 6);
    _charging = true;
    _chargingTimeMax = 1.0*_hulk[4];
    _chargingAtkValue = 0;
  }
  else if(compareArray(hand, _spiderman) && _userID == 200){
    copyArray(_spiderman, _gesture, 6);
    _charging = true;
    _chargingTimeMax = 1.0*_spiderman[4];
    _chargingAtkValue = 0;
  }
  else if(compareArray(hand, _ironman) && _userID == 200){
    copyArray(_ironman, _gesture, 6);
    _charging = true;
    _chargingTimeMax = 1.0*_ironman[4];
    _chargingAtkValue = 0;
  }
  else if(compareArray(hand, _defence) && _userID == 100){
    copyArray(_defence, _gesture, 6);
    _charging = true;
    _chargingTimeMax = 1.0*_defence[4];
    _defValue = 0;
  }
  //otherwise set the gesture to none
  else{
    copyArray(_none, _gesture, 6);
    //if you're at the maximum charge and have opened your palm to indicate sending the attack
    if(_charging){
      _atkValue = _chargingAtkValue;
      _chargingTime = 0;
      _chargingTimeMax = 0;
      _charging = false;
    }
  }
  
  //if we're charging an attack or defence
  if(_charging){
    //If you're at maximum charge but haven't sent the attack or the defense is at max charge
    //don't do anything
    if(_chargingTime >= _chargingTimeMax){
    }
    else{
      //if we're defending
      if(_gesture[0] == _defence[0] && _gesture[1] == _defence[1] && _gesture[2] == _defence[2]){
        //defense scaled depending how long it's been charing.
        _defValue = (1.0*_gesture[3])*(_chargingTime/_chargingTimeMax);
      }
      //if we're attacking
      else{
        _chargingAtkValue = (1.0*_gesture[3])*(_chargingTime/_chargingTimeMax);
      }
      //Increment charging time
      _chargingTime ++;
    }
  }
}

void Player::copyArray(int array[], int copy[], int len){
  for(int i=0; i<len; i++){
    copy[i] = array[i];
  }
}


//Compares two arrays, intended to check if our hand array is the same as
//one of the gesture arrays. Aka are they doing the spiderman hand?
boolean Player::compareArray(int array1[], int array2[]){
  for(int i=0; i<3; i++){
    if(array1[i] != array2[i]){
      return false;
    }
  }
  return true;
}

