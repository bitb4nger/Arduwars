#include "AWGame.h"
#include "ShopData.h"

// The computer player.
//
// Each unit, in turn (artillery first so it shoots before others move in front):
//  1. infantry halfway through a capture keeps capturing
//  2. otherwise it looks at every tile it can reach and every enemy it could
//     hit from there, and scores the attack: damage dealt x enemy price,
//     minus the counter-attack x own price, bonus for a kill
//  3. infantry compares that with capturing a building it can reach
//  4. if there is nothing to hit or take, it walks towards the nearest goal
//     (enemies it can hurt, buildings to capture, else the enemy HQ) using a
//     terrain-aware distance map. The map is computed in the screen buffer,
//     which is free between frames, so it costs no RAM.
// Then it spends its money at every free factory, airport and shipyard.
//
// The computer sees through the fog of war.

static const char LOCA_enemyTurnBanner[] PROGMEM = "ENEMY TURN";

static inline uint8_t manhattan(Point a, Point b){
  return abs(a.x - b.x) + abs(a.y - b.y);
}
static const int8_t PROGMEM aiDirX[4] = {1, 0, -1, 0};
static const int8_t PROGMEM aiDirY[4] = {0, -1, 0, 1};

void AWGame::aiShowUnit(Point tile, uint8_t frames){
  if (tileAt(tile).showsFog) return; // you can't see it, so don't show it
  showScene(tileToCursor(tile), frames);
}

int16_t AWGame::aiAttackScore(const GameUnit &a, Point from, const GameUnit &d){
  GameUnit att = a;
  att.mapPosX = from.x;
  att.mapPosY = from.y;
  uint8_t dmg = calculateDamage(att, d);
  if (dmg > d.healthPoints) dmg = d.healthPoints;
  int16_t price = GameUnit::costsOfUnit(d.type());
  int16_t score = dmg * price / 4 + 1;
  if (dmg == d.healthPoints) {
    score += price + 20;
  }
  else if (canCounter(att, d)) {
    GameUnit def = d;
    def.healthPoints -= dmg;
    uint8_t back = calculateDamage(def, att);
    if (back > att.healthPoints) back = att.healthPoints;
    int16_t myPrice = GameUnit::costsOfUnit(a.type());
    score -= back * myPrice / 4;
    if (back == att.healthPoints) score -= myPrice;
  }
  return score;
}

// distance (in movement points) from every tile to the closest goal of this unit
void AWGame::aiDistanceField(AWPlayer &me, const GameUnit &u, bool hqOnly){
  uint8_t *d = arduboy.getBuffer();
  uint16_t n = mapSize.x * mapSize.y;
  UnitType t = u.type();
  UnitTraits tr = UnitTraits::traitsForUnitType(t);
  AWPlayer &enemy = players[me.idx ^ 1];
  memset(d, 255, n);
  bool any = false;

  if (unitIsInfantry(t) && !hqOnly) {
    for (uint8_t i = 0; i < gameBuildings.getCount(); i++) {
      GameBuilding &b = gameBuildings[i];
      if (b.isOccupied && b.belongsToPlayer == me.idx) continue;
      d[b.mapPosY * mapSize.x + b.mapPosX] = 0;
      any = true;
    }
  }
  if (tr.attackPower && !hqOnly) {
    uint8_t minR = tr.attackRange > 1 ? 2 : 1;
    for (uint8_t i = 0; i < enemy.units.getCount(); i++) {
      GameUnit &e = enemy.units[i];
      if (e.loaded || !GameUnit::canUnitAttackUnit(t, e.type())) continue;
      for (int8_t dy = -tr.attackRange; dy <= tr.attackRange; dy++)
        for (int8_t dx = -tr.attackRange; dx <= tr.attackRange; dx++) {
          uint8_t md = abs(dx) + abs(dy);
          int8_t x = e.mapPosX + dx, y = e.mapPosY + dy;
          if (md < minR || md > tr.attackRange || !inMap(x, y)) continue;
          if (!moveCost(mapTileData[y * mapSize.x + x].type(), t)) continue;
          d[y * mapSize.x + x] = 0;
          any = true;
        }
    }
  }
  if (!any) { // head for the enemy headquarters
    MapTileType hq = me.idx ? MapTileType::P1HQ : MapTileType::P2HQ;
    for (uint8_t i = 0; i < gameBuildings.getCount(); i++)
      if (static_cast<MapTileType>(gameBuildings[i].buildingType) == hq)
        d[gameBuildings[i].mapPosY * mapSize.x + gameBuildings[i].mapPosX] = 0;
  }

  // relax until nothing changes: d[i] = min(d[neighbour] + cost to enter it)
  for (uint8_t pass = 0; pass < 60; pass++) {
    bool changed = false;
    for (uint16_t k = 0; k < n; k++) {
      uint16_t i = (pass & 1) ? (n - 1 - k) : k;
      if (!moveCost(mapTileData[i].type(), t)) continue;
      int8_t x = i % mapSize.x, y = i / mapSize.x;
      uint8_t best = d[i];
      for (uint8_t dir = 0; dir < 4; dir++) {
        int8_t nx = x + (int8_t)pgm_read_byte(aiDirX + dir);
        int8_t ny = y + (int8_t)pgm_read_byte(aiDirY + dir);
        if (!inMap(nx, ny)) continue;
        uint16_t j = ny * mapSize.x + nx;
        if (d[j] >= 250) continue;
        uint8_t c = moveCost(mapTileData[j].type(), t);
        if (c && d[j] + c < best) best = d[j] + c;
      }
      if (best < d[i]) { d[i] = best; changed = true; }
    }
    if (!changed) break;
  }
}

AIPlan AWGame::aiPlanUnit(AWPlayer &me, uint8_t ui){
  GameUnit u = me.units[ui];
  UnitType t = u.type();
  UnitTraits tr = UnitTraits::traitsForUnitType(t);
  Point origin = u.pos();
  AWPlayer &enemy = players[me.idx ^ 1];

  // 1. finish a capture
  int8_t bi = buildingIndexAt(origin);
  if (canCaptureAt(me, t, origin) && gameBuildings[bi].capturePoints() < GameBuilding::FullCapturePoints)
    return {origin, 2, -1};

  markReachable(me, ui);
  auto canStop = [&](Point p) -> bool {
    MapTile &m = tileAt(p);
    return m.showSelection && (p == origin || !m.hasUnit);
  };

  // 2. best attack
  int16_t bestScore = 0;
  int8_t bestTarget = -1;
  Point bestFrom = origin;
  if (tr.attackPower) {
    for (uint8_t i = 0; i < enemy.units.getCount(); i++) {
      GameUnit &e = enemy.units[i];
      if (e.loaded || !GameUnit::canUnitAttackUnit(t, e.type())) continue;
      if (tr.attackRange > 1) {
        uint8_t dist = manhattan(origin, e.pos());
        if (dist < 2 || dist > tr.attackRange) continue;
        int16_t s = aiAttackScore(u, origin, e);
        if (s > bestScore) { bestScore = s; bestTarget = i; bestFrom = origin; }
      }
      else {
        for (uint8_t dir = 0; dir < 4; dir++) {
          Point from = {(int16_t)(e.mapPosX + (int8_t)pgm_read_byte(aiDirX + dir)), (int16_t)(e.mapPosY + (int8_t)pgm_read_byte(aiDirY + dir))};
          if (!inMap(from.x, from.y) || !canStop(from)) continue;
          int16_t s = aiAttackScore(u, from, e) + 2 * terrainDefense(tileAt(from).type(), t);
          if (s > bestScore) { bestScore = s; bestTarget = i; bestFrom = from; }
        }
      }
    }
  }

  // 3. best building to capture
  int16_t capScore = 0;
  Point capPos = origin;
  if (unitIsInfantry(t)) {
    for (uint8_t i = 0; i < gameBuildings.getCount(); i++) {
      GameBuilding &b = gameBuildings[i];
      Point bp = {b.mapPosX, b.mapPosY};
      if ((b.isOccupied && b.belongsToPlayer == me.idx) || !canStop(bp)) continue;
      MapTileType bt = static_cast<MapTileType>(b.buildingType);
      int16_t s = mapTileIsHQ(bt) ? 400 : (mapTileIndexIsShop(bt) ? 120 : 90);
      if (b.isOccupied) s += 30;
      if (s > capScore) { capScore = s; capPos = bp; }
    }
  }

  Point dest = origin;
  uint8_t action = 0; // 0 = wait, 1 = attack, 2 = capture
  if (bestTarget >= 0 && bestScore > capScore) { dest = bestFrom; action = 1; }
  else if (capScore > 0)                        { dest = capPos; action = 2; }
  else if (tr.attackPower || unitIsInfantry(t)) {
    // 4. walk towards the closest goal. If none can be reached from here,
    //    head for the enemy HQ, and failing that at least get off the factory.
    uint8_t *d = arduboy.getBuffer();
    bool found = false;
    for (uint8_t attempt = 0; attempt < 3 && !found; attempt++) {
      if (attempt < 2) aiDistanceField(me, u, attempt == 1);
      int16_t best = 32767;
      for (uint16_t i = 0; i < (uint16_t)(mapSize.x * mapSize.y); i++) {
        Point p = {(int16_t)(i % mapSize.x), (int16_t)(i / mapSize.x)};
        if (!canStop(p)) continue;
        int8_t b = buildingIndexAt(p);
        bool ownShop = b >= 0 && mapTileIndexIsShop(static_cast<MapTileType>(gameBuildings[b].buildingType)) &&
                       gameBuildings[b].isOccupied && gameBuildings[b].belongsToPlayer == me.idx;
        int16_t s;
        if (attempt < 2) {
          if (d[i] == 255) continue;
          s = d[i] * 4 - terrainDefense(tileAt(p).type(), t) + (ownShop ? 6 : 0); // keep factories free
        }
        else {
          if (ownShop) continue;
          s = manhattan(p, origin);
        }
        s = s * 2 + (rnd() & 1);
        if (s < best) { best = s; dest = p; found = true; }
      }
      if (attempt == 2) {
        // only leave if we are blocking our own factory
        int8_t b = buildingIndexAt(origin);
        if (!(b >= 0 && mapTileIndexIsShop(static_cast<MapTileType>(gameBuildings[b].buildingType)))) dest = origin;
      }
    }
  }
  clearMarks();
  return {dest, action, bestTarget};
}

void AWGame::aiBuyUnits(AWPlayer &me){
  AWPlayer &enemy = players[me.idx ^ 1];
  uint8_t infantry = 0, capturable = 0, enemyAir = 0, enemySea = 0;
  for (uint8_t i = 0; i < me.units.getCount(); i++) if (unitIsInfantry(me.units[i].type())) infantry++;
  for (uint8_t i = 0; i < gameBuildings.getCount(); i++)
    if (!(gameBuildings[i].isOccupied && gameBuildings[i].belongsToPlayer == me.idx)) capturable++;
  for (uint8_t i = 0; i < enemy.units.getCount(); i++) {
    MoveClass mc = moveClassOf(enemy.units[i].type());
    if (mc == MoveClass::Air) enemyAir++;
    if (mc == MoveClass::Sea) enemySea++;
  }

  // factories first: their order in the building list is the map order, fine
  for (uint8_t i = 0; i < gameBuildings.getCount(); i++) {
    GameBuilding &b = gameBuildings[i];
    MapTileType bt = static_cast<MapTileType>(b.buildingType);
    Point pos = {b.mapPosX, b.mapPosY};
    if (!b.isOccupied || b.belongsToPlayer != me.idx || !mapTileIndexIsShop(bt)) continue;
    if (me.unitIndexAt(pos) >= 0 || enemy.unitIndexAt(pos) >= 0 || me.units.isFull()) continue;

    static const UnitType PROGMEM ground[] = {UnitType::BigTank, UnitType::Tank, UnitType::Artillery, UnitType::Mech, UnitType::Scout, UnitType::Soldier};
    static const UnitType PROGMEM air[]    = {UnitType::Bomber, UnitType::Heli};
    static const UnitType PROGMEM sea[]    = {UnitType::Battleship, UnitType::Cruiser};
    UnitType pick = UnitType::None;
    const UnitType *list = ground;
    uint8_t len = sizeof(ground);
    if (bt == MapTileType::Factory) {
      if (infantry < 2 || (infantry < 5 && capturable > infantry + 1)) pick = (me.money >= 60 && (rnd() % 3) == 0) ? UnitType::Mech : UnitType::Soldier;
      else if (enemyAir && (rnd() & 1) == 0 && me.money >= GameUnit::costsOfUnit(UnitType::Missiles)) pick = UnitType::Missiles;
      if (pick == UnitType::Soldier || pick == UnitType::Mech) infantry++;
    }
    else if (bt == MapTileType::Airport) {
      if (enemyAir && me.money >= GameUnit::costsOfUnit(UnitType::Fighter)) pick = UnitType::Fighter;
      list = air; len = sizeof(air);
    }
    else {
      if (enemySea && me.money >= GameUnit::costsOfUnit(UnitType::Cruiser)) pick = UnitType::Cruiser;
      list = sea; len = sizeof(sea);
    }
    for (uint8_t k = 0; pick == UnitType::None && k < len; k++) {
      UnitType c = static_cast<UnitType>(pgm_read_byte(list + k));
      // don't blow everything on one big tank early on
      if (c == UnitType::BigTank && (rnd() & 1)) continue;
      if (GameUnit::costsOfUnit(c) <= me.money) pick = c;
    }
    if (pick == UnitType::None || GameUnit::costsOfUnit(pick) > me.money) continue;
    me.money -= GameUnit::costsOfUnit(pick);
    buyUnit(me, pick, pos);
    updateMapForPlayer(viewer);
    aiShowUnit(pos, 20);
  }
}

// Carry out a plan. Kept apart from the planning so the planner's big stack
// frame is gone before the attack animation draws.
void AWGame::aiMoveUnit(AWPlayer &me, uint8_t ui, AIPlan plan){
  Point origin = me.units[ui].pos();
  AWPlayer &enemy = players[me.idx ^ 1];
  if (!(plan.dest == origin)) {
    aiShowUnit(origin, 15);
    animateMove(me, ui, plan.dest);
    moveUnitTo(me, ui, plan.dest);
    updateMapForPlayer(viewer);
    aiShowUnit(plan.dest, 12);
  }
  me.units[ui].activated = GameUnit::UnitStateDisabled;
  if (plan.action == 1) {
    attackUnit(me, ui, enemy, plan.target);
  }
  else if (plan.action == 2) {
    aiShowUnit(plan.dest, 10);
    if (captureBuilding(me, ui)) { updateMapForPlayer(viewer); aiShowUnit(plan.dest, 30); }
  }
  resetAbandonedCaptures();
  updateMapForPlayer(viewer);
}

void AWGame::doComputerTurn(AWPlayer &p){
  renderScene(tileToCursor(players[0].cursorIndex), false, -1, p.idx);
  showBanner(LOCA_enemyTurnBanner, 45);

  for (uint8_t pass = 0; pass < 2; pass++) {
    for (uint8_t i = 0; i < p.units.getCount(); ) {
      GameUnit &u = p.units[i];
      bool indirect = UnitTraits::traitsForUnitType(u.type()).attackRange > 1;
      if (!u.activated || u.loaded || (pass == 0) != indirect) { i++; continue; }
      uint8_t before = p.units.getCount();
      aiMoveUnit(p, i, aiPlanUnit(p, i));
      if (checkWinner()) return;
      if (p.units.getCount() < before) continue; // it died: the next unit moved into slot i
      i++;
    }
  }
  aiBuyUnits(p);
  updateMapForPlayer(viewer);
}
