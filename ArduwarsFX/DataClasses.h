#ifndef DATA_H
#define DATA_H

#include <Arduboy2.h>

// Arduwars data types: units, terrain, buildings, players and map tiles.
// Everything is packed into bitfields because the Arduboy only has 2.5 KB RAM.

static inline bool operator==(const Point &a, const Point &b){ return a.x == b.x && a.y == b.y; }
static inline Point operator*(const Point &a, int16_t m){ return {(int16_t)(a.x * m), (int16_t)(a.y * m)}; }

enum class UnitType : int8_t {
  Soldier = 0,
  Mech,
  SpecOps,
  Scout,
  Assist,        // Heals adjacent units at the start of the turn and carries one infantry
  Tank,
  BigTank,
  Artillery,
  Rocket,        // Can only shoot planes and ships
  Missiles,
  Heli,
  Fighter,       // Can only fight planes
  Bomber,
  Cruiser,       // Can only fight ships
  Battleship,
  Transportship, // Carries one infantry
  None = -1
};

enum class MapTileType : uint8_t {
  Water      = 0,
  Reef       = 1,
  Coast1     = 2,
  Coast12    = 13,
  Plains     = 14,
  Grass      = 15,
  Street1    = 16,
  Street6    = 21,
  Hill       = 22,
  Mountain   = 23,
  Forest     = 24,
  City       = 25,
  Factory    = 26,
  Airport    = 27,
  Shipyard   = 28,
  ScienceLab = 29,
  P1HQ       = 30,
  P2HQ       = 31
};

static inline bool mapTileIndexIsBuilding(MapTileType t){
  return (t >= MapTileType::City && t <= MapTileType::P2HQ);
}
static inline bool mapTileIndexIsShop(MapTileType t){
  return (t == MapTileType::Factory || t == MapTileType::Airport || t == MapTileType::Shipyard);
}
static inline bool mapTileIsOpaque(MapTileType t){
  return (t == MapTileType::Hill || t == MapTileType::Mountain);
}
static inline bool mapTileIsHQ(MapTileType t){
  return (t == MapTileType::P1HQ || t == MapTileType::P2HQ);
}

// How a unit moves: decides which terrain it may enter and what it costs.
enum class MoveClass : uint8_t { Infantry, Ground, Air, Sea };

static inline MoveClass moveClassOf(UnitType t){
  if (t <= UnitType::SpecOps)  return MoveClass::Infantry;
  if (t <= UnitType::Missiles) return MoveClass::Ground;
  if (t <= UnitType::Bomber)   return MoveClass::Air;
  return MoveClass::Sea;
}
static inline bool unitIsInfantry(UnitType t){ return t >= UnitType::Soldier && t <= UnitType::SpecOps; }
static inline bool unitIsTransport(UnitType t){ return t == UnitType::Assist || t == UnitType::Transportship; }

// Movement cost of entering a tile, 0 = impossible.
uint8_t moveCost(MapTileType tile, UnitType unit);

// Defense stars a tile gives to a unit standing on it (0..3). Air units get none.
uint8_t terrainDefense(MapTileType tile, UnitType unit);

// Who can attack whom. Bit (15 - defender) is set when the attacker can hit the defender.
constexpr uint16_t PROGMEM attackLookupTable[16] =
{
  0b1111111111000000, // Soldier
  0b1111111111000000, // Mech
  0b1111111111000000, // SpecOps
  0b1111111111000000, // Scout
  0b0000000000000000, // Assist
  0b1111111111000000, // Tank
  0b1111111111000000, // BigTank
  0b1111111111000111, // Artillery
  0b0000000000111111, // Rocket
  0b1111111111111111, // Missiles
  0b1111111111111111, // Heli
  0b0000000000111000, // Fighter
  0b1111111111000111, // Bomber
  0b0000000000000111, // Cruiser
  0b1111111111000111, // Battleship
  0b0000000000000000, // Transportship
};

class UnitTraits{
public:
  uint8_t moveDistance:3; // tiles of movement (terrain costs apply)
  uint8_t attackPower :5;
  uint8_t attackRange :3; // 1 = direct. >1 = indirect: fires at 2..range and can't move and fire
  uint8_t defense     :5;

  static UnitTraits traitsForUnitType(UnitType unitType);
};

// {move, attack, range, defense}
constexpr UnitTraits PROGMEM allUnitTraits[16] = {
  {3,1,1,0}, // Soldier
  {2,3,1,0}, // Mech
  {5,3,1,0}, // SpecOps
  {6,3,1,1}, // Scout
  {6,0,0,1}, // Assist
  {5,5,1,2}, // Tank
  {3,7,1,3}, // BigTank
  {4,4,3,1}, // Artillery
  {4,5,4,1}, // Rocket
  {4,6,5,0}, // Missiles
  {6,3,1,1}, // Heli
  {7,4,1,2}, // Fighter
  {5,7,1,3}, // Bomber
  {5,3,1,1}, // Cruiser
  {5,5,4,3}, // Battleship
  {6,0,0,1}, // Transportship
};

class GameUnit{
public:
  // laid out so no field crosses a byte boundary (much less code on AVR)
  uint8_t unitType    :4;
  uint8_t healthPoints:4; // 1..10
  uint8_t mapPosX     :5;
  uint8_t activated   :1; // UnitStateActive until the unit has acted this turn
  uint8_t loaded      :1; // riding inside a transport (sits on the transport's tile, not drawn)
  uint8_t carrying    :1; // transport with a passenger
  uint8_t mapPosY     :5;
  uint8_t unused      :3;

  GameUnit();

  static uint8_t costsOfUnit(UnitType unitType);
  static bool canUnitAttackUnit(UnitType attacker, UnitType defender);

  UnitType type() const { return static_cast<UnitType>(unitType); }
  Point pos() const { return {mapPosX, mapPosY}; }

  static constexpr uint8_t UnitStateActive    = 1;
  static constexpr uint8_t UnitStateDisabled  = 0;
  static constexpr uint8_t UnitDefaultHealth  = 10;
};

class GameBuilding{
public:
  uint8_t buildingType    :5; // MapTileType
  uint8_t isOccupied      :1; // owned by someone
  uint8_t belongsToPlayer :1; // 0 = player 1, 1 = player 2
  uint8_t spare           :1;
  uint8_t mapPosX         :5;
  uint8_t capLow          :3;
  uint8_t mapPosY         :5;
  uint8_t capHigh         :3;

  // capture points: 20 = untouched. Infantry knocks off its HP each capture.
  uint8_t capturePoints() const { return capLow | (capHigh << 3); }
  void setCapturePoints(uint8_t v){ capLow = v & 7; capHigh = v >> 3; }

  GameBuilding();
  static constexpr uint8_t buildingViewDistance = 3;
  static constexpr uint8_t FullCapturePoints = 20;
};

// A tiny fixed-size list.
template<typename T, uint8_t N> class FixedList {
  T items[N];
  uint8_t count = 0;
public:
  uint8_t getCount() const { return count; }
  bool isFull() const { return count >= N; }
  void clear(){ count = 0; }
  void add(const T &t){ if (count < N) items[count++] = t; }
  void removeAt(uint8_t i){ for (uint8_t j = i; j + 1 < count; j++) items[j] = items[j+1]; count--; }
  T & operator[](uint8_t i){ return items[i]; }
  const T & operator[](uint8_t i) const { return items[i]; }
};

class AWPlayer{
public:
  uint8_t idx;            // 0 or 1
  bool hadUnits;          // you only lose by "no units left" once you had some
  uint16_t money;         // shown x100
  Point cursorIndex;
  FixedList<GameUnit, 20> units;   // 20 units a side

  AWPlayer(uint8_t i) : idx(i) { reset(); }
  void reset();
  // the unit standing on a tile (passengers are skipped), -1 if none
  int8_t unitIndexAt(Point coordinates) const;

  static constexpr uint8_t BaseIncome = 10; // per city / HQ, x100
};

class MapTile {
public:
  static constexpr uint8_t BelongsToPlayer1 = 0;
  static constexpr uint8_t BelongsToPlayer2 = 1;
  static constexpr uint8_t BelongsToPlayer  = 0; // building flags are relative to the viewing player
  static constexpr uint8_t BelongsToEnemy   = 1;

  uint8_t tileID:5;
  uint8_t buildingIsOccupied:1;
  uint8_t buildingBelongsTo:1;
  uint8_t hasUnit:1;
  uint8_t unitBelongsTo:1;
  uint8_t showSelection:1;
  uint8_t showsFog:1;
  uint8_t unitIsActive:1;
  uint8_t unitSpriteID:4;    // unit type when hasUnit; scratch for the movement fill otherwise

  MapTileType type() const { return static_cast<MapTileType>(tileID); }
};

#endif
