#include "DataClasses.h"
#include "ShopData.h"

GameUnit::GameUnit(){
  unitType = static_cast<uint8_t>(UnitType::Soldier);
  healthPoints = UnitDefaultHealth;
  mapPosX = mapPosY = 0;
  activated = UnitStateActive;
  loaded = carrying = 0;
  unused = 0;
}

uint8_t GameUnit::costsOfUnit(UnitType unitType){
  return pgm_read_byte(unitPrices + static_cast<uint8_t>(unitType));
}

bool GameUnit::canUnitAttackUnit(UnitType attacker, UnitType defender){
  if (attacker == UnitType::None || defender == UnitType::None) return false;
  uint16_t lut = pgm_read_word(attackLookupTable + static_cast<uint8_t>(attacker));
  return (lut & (0x8000u >> static_cast<uint8_t>(defender))) != 0;
}

UnitTraits UnitTraits::traitsForUnitType(UnitType unitType){
  UnitTraits traits;
  memcpy_P(&traits, &allUnitTraits[static_cast<uint8_t>(unitType)], sizeof(traits));
  return traits;
}

GameBuilding::GameBuilding(){
  buildingType = static_cast<uint8_t>(MapTileType::City);
  setCapturePoints(FullCapturePoints);
  isOccupied = belongsToPlayer = 0;
  mapPosX = mapPosY = 0;
  spare = 0;
}

void AWPlayer::reset(){
  money = 30;
  hadUnits = false;
  cursorIndex = {0,0};
  units.clear();
}

int8_t AWPlayer::unitIndexAt(Point c) const {
  for (uint8_t i = 0; i < units.getCount(); i++) {
    const GameUnit &u = units[i];
    if (!u.loaded && u.mapPosX == c.x && u.mapPosY == c.y) return i;
  }
  return -1;
}

uint8_t moveCost(MapTileType tile, UnitType unit){
  MoveClass mc = moveClassOf(unit);
  if (mc == MoveClass::Air) return 1;
  bool water = tile <= MapTileType::Coast12;
  if (mc == MoveClass::Sea) {
    if (tile == MapTileType::Reef) return 0;
    return (water || tile == MapTileType::Shipyard) ? 1 : 0;
  }
  if (water) return 0;
  switch (tile) {
    case MapTileType::Forest:   return (mc == MoveClass::Infantry) ? 1 : 2;
    case MapTileType::Hill:     return (mc == MoveClass::Infantry) ? ((unit == UnitType::Soldier) ? 2 : 1) : 0;
    case MapTileType::Mountain: return (mc == MoveClass::Infantry) ? ((unit == UnitType::Soldier) ? 3 : 2) : 0;
    default:                    return 1;
  }
}

uint8_t terrainDefense(MapTileType tile, UnitType unit){
  if (moveClassOf(unit) == MoveClass::Air) return 0;
  switch (tile) {
    case MapTileType::Forest:
    case MapTileType::Reef:     return 1;
    case MapTileType::Hill:     return 2;
    case MapTileType::Mountain: return 3;
    case MapTileType::P1HQ:
    case MapTileType::P2HQ:     return 3;
    default: return mapTileIndexIsBuilding(tile) ? 2 : 0;
  }
}
