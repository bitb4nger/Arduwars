#ifndef SHOP_H
#define SHOP_H

#include <avr/pgmspace.h>
#include "DataClasses.h"

// Prices in 100s (10 = 1000). They fit a byte, so the max is 25500.
const uint8_t unitPrices[] PROGMEM = {
  10,  // Soldier
  30,  // Mech
  50,  // SpecOps
  40,  // Scout
  50,  // Assist
  70,  // Tank
  160, // BigTank
  60,  // Artillery
  150, // Rocket
  120, // Missiles
  90,  // Heli
  200, // Fighter
  220, // Bomber
  180, // Cruiser
  255, // Battleship
  120  // Transportship
};

const UnitType buyableUnitsAtFactory[] PROGMEM = {
  UnitType::Soldier, UnitType::Mech, UnitType::SpecOps, UnitType::Scout, UnitType::Assist,
  UnitType::Tank, UnitType::BigTank, UnitType::Artillery, UnitType::Rocket, UnitType::Missiles
};
constexpr uint8_t NumberOfBuyableUnitsAtFactory = sizeof(buyableUnitsAtFactory);

const UnitType buyableUnitsAtAirPort[] PROGMEM = {
  UnitType::Heli, UnitType::Fighter, UnitType::Bomber
};
constexpr uint8_t NumberOfBuyableUnitsAtAirport = sizeof(buyableUnitsAtAirPort);

const UnitType buyableUnitsAtShipyard[] PROGMEM = {
  UnitType::Transportship, UnitType::Cruiser, UnitType::Battleship
};
constexpr uint8_t NumberOfBuyableUnitsAtShipyard = sizeof(buyableUnitsAtShipyard);

#endif
