#include "AWGame.h"
#include "Localization.h"
#include "ShopData.h"

// Arduwars FX: the game flow, the rules and everything that draws.
// The computer player lives in AWAI.cpp.
//
// Buttons: B = select / confirm, A = cancel (hold A to scroll faster).

#define FS(s) reinterpret_cast<const __FlashStringHelper *>(s)

// beep pitches, worked out at compile time (BeepPin1::freq uses floats)
static constexpr uint16_t BeepSelect  = BeepPin1::freq(880);
static constexpr uint16_t BeepMenu    = BeepPin1::freq(660);
static constexpr uint16_t BeepHit     = BeepPin1::freq(220);
static constexpr uint16_t BeepKill    = BeepPin1::freq(110);
static constexpr uint16_t BeepCapture = BeepPin1::freq(1320);

static inline uint8_t manhattan(Point a, Point b){
  return abs(a.x - b.x) + abs(a.y - b.y);
}

static const int8_t PROGMEM dirX[4] = {1, 0, -1, 0};
static const int8_t PROGMEM dirY[4] = {0, -1, 0, 1};

// =====================================================================
// Main loop and menus

void AWGame::run(void){
  arduboy.beginDoFirst();   // boot, flashlight, system buttons - no boot logo, the FX menu shows ours
  arduboy.waitNoButtons();
  FX::begin(FX_DATA_PAGE, FX_SAVE_PAGE);
  FX::setFont(fontBlack, dcmMasked);
  FX::setCursorRange(0, 32767);
  arduboy.setFrameRate(60);
  beep.begin();
  rngState = arduboy.generateRandomSeed() | 1;

  while(true){
    switch (gameState) {
      case AWGameState::continueGame:
        if (loadSave()) playGame(true);
        gameState = AWGameState::showMainMenu;
        break;
      case AWGameState::playSinglePlayer:
      case AWGameState::playMultiPlayer: {
        vsComputer = (gameState == AWGameState::playSinglePlayer);
        int8_t map = showMapSelection();
        if (map >= 0) { mapIndex = map; playGame(false); }
        gameState = AWGameState::showMainMenu;
        break;
      }
      default:
        gameState = showMainMenu();
    }
  }
}

AWGameState AWGame::showMainMenu(){
  static char_P LOCA_continue[] PROGMEM = "CONTINUE";
  hasSave = loadSave();
  uint8_t first = hasSave ? 0 : 1;      // item 0 (CONTINUE) only with a save
  int8_t cursorIdx = first;
  constexpr uint8_t yOffset = 30;

  while(true){
    if(!nextFrame()) continue;
    arduboy.pollButtons();

    if (arduboy.justPressed(UP_BUTTON))   cursorIdx = (cursorIdx > first) ? cursorIdx - 1 : 3;
    if (arduboy.justPressed(DOWN_BUTTON)) cursorIdx = (cursorIdx < 3) ? cursorIdx + 1 : first;
    if (arduboy.justPressed(B_BUTTON)){
      rngState ^= arduboy.frameCount;
      if (cursorIdx == 0) return AWGameState::continueGame;
      if (cursorIdx == 1) return AWGameState::playSinglePlayer;
      if (cursorIdx == 2) return AWGameState::playMultiPlayer;
      arduboy.audio.toggle();
      arduboy.audio.saveOnOff();
    }

    arduboy.clear();
    FX::drawBitmap(7, 4, logoGfx, 0, dbmWhite);
    tinyfont.setTextColor(WHITE);
    char_P *items[4] = {LOCA_continue, LOCA_SinglePlayer, LOCA_MultiPlayer, arduboy.audio.enabled() ? LOCA_Sound_on : LOCA_Sound_off};
    for (uint8_t i = first; i < 4; i++) {
      tinyfont.setCursor(10, yOffset + 8 * i);
      tinyfont.print(FS(items[i]));
    }
    tinyfont.setCursor(2, yOffset + 8 * cursorIdx); tinyfont.print(FS(LOCA_Cursor));
    tinyfont.setTextColor(BLACK);
    FX::display();
  }
}

int8_t AWGame::showMapSelection(){
  int8_t cursorIdx = 0;
  constexpr uint8_t yOffset = 36;
  while(true){
    if(!nextFrame()) continue;
    arduboy.pollButtons();
    if (arduboy.justPressed(UP_BUTTON))   cursorIdx = (cursorIdx + 2) % 3;
    if (arduboy.justPressed(DOWN_BUTTON)) cursorIdx = (cursorIdx + 1) % 3;
    if (arduboy.justPressed(B_BUTTON)) return cursorIdx;
    if (arduboy.justPressed(A_BUTTON)) return -1;

    arduboy.clear();
    FX::drawBitmap(7, 4, logoGfx, 0, dbmWhite);
    tinyfont.setTextColor(WHITE);
    tinyfont.setCursor(10, 24);          tinyfont.print(FS(vsComputer ? LOCA_SinglePlayer : LOCA_MultiPlayer));
    tinyfont.setCursor(10, yOffset);     tinyfont.print(FS(LOCA_mapSmall));
    tinyfont.setCursor(10, yOffset + 8); tinyfont.print(FS(LOCA_mapMedium));
    tinyfont.setCursor(10, yOffset + 16);tinyfont.print(FS(LOCA_mapBig));
    tinyfont.setCursor(2, yOffset + 8 * cursorIdx); tinyfont.print(FS(LOCA_Cursor));
    tinyfont.setCursor(64, 58);          tinyfont.print(FS(LOCA_BackWithA));
    tinyfont.setTextColor(BLACK);
    FX::display();
  }
}

// =====================================================================
// A whole battle

void AWGame::playGame(bool resume){
  if (!resume) {
    daysPlayed = 1;
    turnPlayer = 0;
    players[0].reset();
    players[1].reset();
  }
  loadMap(mapIndex == 0 ? mapSkirmish : (mapIndex == 1 ? mapBattle : mapArduwars));
  if (resume) loadSave(); // loadMap reset the buildings; put the saved ones back
#ifdef AW_SCENARIO
  // test setup on the skirmish map: a small fight next to player 1's HQ
  buyUnit(players[0], UnitType::Tank, {2, 2});
  buyUnit(players[0], UnitType::Artillery, {1, 3});
  buyUnit(players[0], UnitType::Assist, {1, 6});
  buyUnit(players[0], UnitType::Soldier, {1, 7});
  buyUnit(players[1], UnitType::Soldier, {2, 4});
  buyUnit(players[1], UnitType::Mech, {3, 3});
  buyUnit(players[1], UnitType::Tank, {1, 5});
#endif

  bool newTurn = !resume;
  uint8_t winner = 0;
  while (true) {
    uint8_t cur = turnPlayer;
    AWPlayer &p = players[cur];
    if (newTurn) beginTurn(p);
    newTurn = true;

#ifdef AW_AUTOPLAY
    if (true) {   // test build: the computer plays both sides
#else
    if (vsComputer && cur == 1) {
#endif
      viewer = 0;
      updateMapForPlayer(viewer);
      doComputerTurn(p);
    }
    else {
      viewer = cur;
      makeScreenTransition();
      if (!vsComputer) showDialog(cur ? LOCA_player2 : LOCA_player1);
      updateMapForPlayer(viewer);
      saveMagic = SaveMagic;
      saveGame();                  // autosave at the start of every human turn
      if (!doHumanTurn(p)) return; // quit (the autosave stays)
    }

    winner = checkWinner();
#ifdef AW_AUTOPLAY
    Serial.print(F("D")); Serial.print(daysPlayed); Serial.print(F(" P")); Serial.print(cur + 1);
    for (uint8_t pi = 0; pi < 2; pi++) {
      uint8_t owned = 0;
      for (uint8_t i = 0; i < gameBuildings.getCount(); i++) owned += gameBuildings[i].isOccupied && gameBuildings[i].belongsToPlayer == pi;
      Serial.print(F(" | u")); Serial.print(players[pi].units.getCount());
      Serial.print(F(" b")); Serial.print(owned);
      Serial.print(F(" m")); Serial.print(players[pi].money);
    }
    Serial.println();
    if (cur == 1 && daysPlayed % 5 == 0) {
      Serial.print(F("S"));
      for (uint8_t pi = 0; pi < 2; pi++)
        for (uint8_t i = 0; i < players[pi].units.getCount(); i++) {
          GameUnit &u = players[pi].units[i];
          Serial.print(' '); Serial.print(pi); Serial.print(','); Serial.print(u.unitType); Serial.print(',');
          Serial.print(u.mapPosX); Serial.print(','); Serial.print(u.mapPosY); Serial.print(','); Serial.print(u.healthPoints);
        }
      Serial.print(F(" B"));
      for (uint8_t i = 0; i < gameBuildings.getCount(); i++) {
        GameBuilding &b = gameBuildings[i];
        Serial.print(' '); Serial.print(b.mapPosX); Serial.print(','); Serial.print(b.mapPosY); Serial.print(',');
        Serial.print(b.isOccupied ? b.belongsToPlayer : 9);
      }
      Serial.println();
    }
    if (winner) { Serial.print(F("WINNER ")); Serial.println(winner); }
#endif
    if (winner) break;

    turnPlayer ^= 1;
    if (turnPlayer == 0 && ++daysPlayed > 99) break; // stalemate after 99 days
  }
  saveMagic = 0;
  saveGame();                      // the battle is over: no more CONTINUE
  showGameOver(winner);
}

void AWGame::showGameOver(uint8_t winner){
  char_P *text = LOCA_draw;
  if (winner == 1) text = vsComputer ? LOCA_youWin : LOCA_p1Wins;
  if (winner == 2) text = vsComputer ? LOCA_youLose : LOCA_p2Wins;
  viewer = (winner == 2 && !vsComputer) ? 1 : 0;
  updateMapForPlayer(viewer);
  renderScene(tileToCursor(players[viewer].cursorIndex), false, -1, viewer);
  showDialog(text);
}

// Income, repairs and fresh units at the start of a player's turn.
void AWGame::beginTurn(AWPlayer &p){
  for (uint8_t i = 0; i < gameBuildings.getCount(); i++) {
    GameBuilding &b = gameBuildings[i];
    if (!b.isOccupied || b.belongsToPlayer != p.idx) continue;
    MapTileType t = static_cast<MapTileType>(b.buildingType);
    if (t == MapTileType::City || mapTileIsHQ(t)) p.money += AWPlayer::BaseIncome;
  }
  if (p.money > 999) p.money = 999;

  for (uint8_t i = 0; i < p.units.getCount(); i++) {
    GameUnit &u = p.units[i];
    u.activated = GameUnit::UnitStateActive;
    if (u.loaded) continue;
    // units on their own buildings get repaired
    int8_t bi = buildingIndexAt(u.pos());
    if (bi >= 0 && gameBuildings[bi].isOccupied && gameBuildings[bi].belongsToPlayer == p.idx)
      u.healthPoints = min(GameUnit::UnitDefaultHealth, u.healthPoints + 2);
    // the assist vehicle patches up its neighbours
    if (u.type() == UnitType::Assist) {
      for (uint8_t d = 0; d < 4; d++) {
        int8_t j = p.unitIndexAt({(int16_t)(u.mapPosX + (int8_t)pgm_read_byte(dirX + d)), (int16_t)(u.mapPosY + (int8_t)pgm_read_byte(dirY + d))});
        if (j >= 0 && p.units[j].healthPoints < GameUnit::UnitDefaultHealth) p.units[j].healthPoints++;
      }
    }
  }
}

uint8_t AWGame::checkWinner(){
  for (uint8_t pi = 0; pi < 2; pi++) {
    AWPlayer &p = players[pi];
    bool lost = p.hadUnits && p.units.getCount() == 0;
    MapTileType hq = pi ? MapTileType::P2HQ : MapTileType::P1HQ;
    for (uint8_t i = 0; i < gameBuildings.getCount(); i++) {
      GameBuilding &b = gameBuildings[i];
      if (static_cast<MapTileType>(b.buildingType) == hq && b.belongsToPlayer != pi) lost = true;
    }
    if (lost) return pi ? 1 : 2;
  }
  return 0;
}

// =====================================================================
// The human player's turn

bool AWGame::doHumanTurn(AWPlayer &p){
#ifdef AW_TESTHOOKS
  Serial.print(F("HT")); Serial.print(daysPlayed); Serial.print(' ');
#endif
  hCursor = tileToCursor(p.cursorIndex);
  hState = AWTurnState::Default;
  hSel = -1;

  while (true) {
    if(!nextFrame()) continue;
    arduboy.pollButtons();

    if (hState == AWTurnState::UnitAttack || hState == AWTurnState::UnitDrop) {
      // jump between the marked tiles
      int8_t dir = 0;
      if (arduboy.justPressed(DOWN_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) dir = 1;
      if (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) dir = -1;
      if (dir) {
        Point n = nextMarkedMapPosition(cursorToTile(hCursor), dir);
        if (n.x >= 0) hCursor = tileToCursor(n);
      }
    }
    else {
      uint8_t speed = arduboy.pressed(A_BUTTON) ? 4 : 2;
      if (arduboy.pressed(UP_BUTTON))    hCursor.y -= speed;
      if (arduboy.pressed(DOWN_BUTTON))  hCursor.y += speed;
      if (arduboy.pressed(LEFT_BUTTON))  hCursor.x -= speed;
      if (arduboy.pressed(RIGHT_BUTTON)) hCursor.x += speed;
      if (hCursor.x < -8) hCursor.x = -8;
      if (hCursor.y < -8) hCursor.y = -8;
      if (hCursor.x > mapSize.x * TILE_SIZE - 24) hCursor.x = mapSize.x * TILE_SIZE - 24;
      if (hCursor.y > mapSize.y * TILE_SIZE - 24) hCursor.y = mapSize.y * TILE_SIZE - 24;
    }

    if (arduboy.justPressed(A_BUTTON)) {
      if (hState == AWTurnState::UnitSelected) {
        clearMarks();
        hSel = -1;
        hState = AWTurnState::Default;
        hCursor = tileToCursor(hOrigin);
      }
      else if (hState == AWTurnState::UnitAttack || hState == AWTurnState::UnitDrop) {
        moveUnitTo(p, hSel, hOrigin);
        updateMapForPlayer(viewer);
        markReachable(p, hSel);
        hCursor = tileToCursor(hDest);
        hState = AWTurnState::UnitSelected;
      }
    }

    if (arduboy.justPressed(B_BUTTON)) {
      sfx(BeepSelect, 3);
      uint8_t r = humanPressB(p);
      if (r == 1) return true;  // turn over (or game won)
      if (r == 2) return false; // quit
    }

    int8_t ghost = (hState == AWTurnState::UnitSelected && hSel >= 0) ? (int8_t)p.units[hSel].unitType : -1;
    renderScene(hCursor, true, ghost, p.idx);
    FX::display();
  }
}

// What the B button does during your turn. 0 = carry on, 1 = turn is over, 2 = quit.
uint8_t AWGame::humanPressB(AWPlayer &p){
  Point tile = cursorToTile(hCursor);
  hCursor = tileToCursor(tile); // snap to the grid
  bool finished = false;

  switch (hState) {
    case AWTurnState::Default: {
      int8_t ui = p.unitIndexAt(tile);
      MapTile t = tileAt(tile);
      int8_t bi = buildingIndexAt(tile);
      if (ui >= 0 && p.units[ui].activated) {
        hSel = ui;
        hOrigin = tile;
        markReachable(p, ui);
        hState = AWTurnState::UnitSelected;
      }
      else if (ui < 0 && !t.hasUnit && mapTileIndexIsShop(t.type()) && bi >= 0 &&
               gameBuildings[bi].isOccupied && gameBuildings[bi].belongsToPlayer == p.idx) {
        if (p.units.isFull()) showDialog(LOCA_Unit_limit_reached);
        else {
          UnitType bought = showShopForBuildingAndPlayer(t.type(), p);
          if (bought != UnitType::None) {
            p.money -= GameUnit::costsOfUnit(bought);
            buyUnit(p, bought, tile);
            updateMapForPlayer(viewer);
          }
        }
      }
      else {
        int8_t o = turnMenu();
        if (o == 0) { p.cursorIndex = tile; return 1; }
        if (o == 2) return 2;
      }
      return 0;
    }

    case AWTurnState::UnitSelected: {
      if (!tileAt(tile).showSelection) return 0;
      hDest = tile;
      int8_t other = p.unitIndexAt(hDest);
      animateMove(p, hSel, hDest);
      uint8_t choice = chooseUnitAction(p, hSel, hOrigin, hDest);
      if (choice != ActCancel && choice != ActLoad) moveUnitTo(p, hSel, hDest);

      switch (choice) {
        case ActFire:
          updateMapForPlayer(viewer);
          markTargets(p, p.units[hSel], hDest, hOrigin, false);
          hCursor = tileToCursor(nextMarkedMapPosition({0, 0}, 1));
          hState = AWTurnState::UnitAttack;
          return 0;
        case ActDrop:
          updateMapForPlayer(viewer);
          markDropTiles(p.units[passengerOf(p, hSel)].type(), hDest);
          hCursor = tileToCursor(nextMarkedMapPosition(hDest, 1));
          hState = AWTurnState::UnitDrop;
          return 0;
        case ActCapture:
          if (captureBuilding(p, hSel)) { sfx(BeepCapture, 15); showBanner(LOCA_captured, 40); }
          finished = true;
          break;
        case ActLoad: {
          GameUnit &cargo = p.units[hSel];
          cargo.mapPosX = hDest.x; cargo.mapPosY = hDest.y;
          cargo.loaded = 1;
          p.units[other].carrying = 1;
          finished = true;
          break;
        }
        case ActWait:
          finished = true;
          break;
        default:
          hSel = -1;
          hState = AWTurnState::Default;
          hCursor = tileToCursor(hOrigin);
          updateMapForPlayer(viewer);
          return 0;
      }
      break;
    }

    case AWTurnState::UnitAttack: {
      if (!tileAt(tile).showSelection) return 0;
      AWPlayer &enemy = players[p.idx ^ 1];
      int8_t di = enemy.unitIndexAt(tile);
      if (di < 0) return 0;
      clearMarks();
      if (!attackUnit(p, hSel, enemy, di)) hSel = -1;
      finished = true;
      break;
    }

    case AWTurnState::UnitDrop: {
      if (!tileAt(tile).showSelection) return 0;
      GameUnit &cargo = p.units[passengerOf(p, hSel)];
      cargo.loaded = 0;
      cargo.mapPosX = tile.x; cargo.mapPosY = tile.y;
      cargo.activated = GameUnit::UnitStateDisabled;
      p.units[hSel].carrying = 0;
      finished = true;
      break;
    }
  }

  if (finished) {
    if (hSel >= 0) p.units[hSel].activated = GameUnit::UnitStateDisabled;
    hSel = -1;
    clearMarks();
    resetAbandonedCaptures();
    updateMapForPlayer(viewer);
    hState = AWTurnState::Default;
    if (checkWinner()) return 1;
  }
  return 0;
}

// The menu after moving a unit. Its own function so the option list is off
// the stack again before any attack animation runs.
uint8_t AWGame::chooseUnitAction(AWPlayer &p, int8_t sel, Point origin, Point dest){
  GameUnit u = p.units[sel];
  int8_t other = p.unitIndexAt(dest);
  char_P* options[5];
  uint8_t ids[5];
  uint8_t n = 0;
  if (other >= 0 && other != sel) {
    options[n] = LOCA_load; ids[n++] = ActLoad;
  }
  else {
    if (markTargets(p, u, dest, origin, false)) { options[n] = LOCA_attack; ids[n++] = ActFire; }
    if (canCaptureAt(p, u.type(), dest))         { options[n] = LOCA_capture; ids[n++] = ActCapture; }
    int8_t pass = passengerOf(p, sel);
    if (pass >= 0 && markDropTiles(p.units[pass].type(), dest)) { options[n] = LOCA_drop; ids[n++] = ActDrop; }
    options[n] = LOCA_wait; ids[n++] = ActWait;
  }
  options[n] = LOCA_cancel; ids[n++] = ActCancel;
  clearMarks();
  int8_t o = showOptions(options, n);
  return (o < 0) ? ActCancel : ids[o];
}

int8_t AWGame::turnMenu(){
  char_P* options[3] = {LOCA_endTurn, LOCA_cancel, LOCA_quit};
  return showOptions(options, 3);
}

// =====================================================================
// Rules

int8_t AWGame::buildingIndexAt(Point pos){
  for (uint8_t i = 0; i < gameBuildings.getCount(); i++)
    if (gameBuildings[i].mapPosX == pos.x && gameBuildings[i].mapPosY == pos.y) return i;
  return -1;
}

int8_t AWGame::passengerOf(AWPlayer &p, uint8_t ui){
  GameUnit &t = p.units[ui];
  if (!t.carrying) return -1;
  for (uint8_t i = 0; i < p.units.getCount(); i++) {
    GameUnit &u = p.units[i];
    if (u.loaded && u.mapPosX == t.mapPosX && u.mapPosY == t.mapPosY) return i;
  }
  return -1;
}

void AWGame::moveUnitTo(AWPlayer &p, uint8_t ui, Point to){
  int8_t pass = passengerOf(p, ui);
  if (pass >= 0) { p.units[pass].mapPosX = to.x; p.units[pass].mapPosY = to.y; }
  p.units[ui].mapPosX = to.x;
  p.units[ui].mapPosY = to.y;
}

void AWGame::removeUnit(AWPlayer &p, uint8_t ui){
  int8_t pass = passengerOf(p, ui);
  if (pass >= 0) {
    p.units.removeAt(pass);
    if (pass < ui) ui--;
  }
  p.units.removeAt(ui);
}

bool AWGame::buyUnit(AWPlayer &p, UnitType t, Point pos){
  if (p.units.isFull()) return false;
  GameUnit u;
  u.unitType = static_cast<uint8_t>(t);
  u.mapPosX = pos.x;
  u.mapPosY = pos.y;
  u.activated = GameUnit::UnitStateDisabled;
  p.units.add(u);
  p.hadUnits = true;
  return true;
}

// Damage in HP. Attack power and the attacker's health against the
// defender's armour plus the terrain it stands on.
uint8_t AWGame::calculateDamage(const GameUnit &attacker, const GameUnit &defender){
  static const uint8_t PROGMEM factor[6] = {100, 80, 60, 45, 35, 25};
  UnitTraits ta = UnitTraits::traitsForUnitType(attacker.type());
  UnitTraits td = UnitTraits::traitsForUnitType(defender.type());
  uint8_t stars = td.defense + terrainDefense(tileAt(defender.pos()).type(), defender.type());
  if (stars > 5) stars = 5;
  uint16_t num = (uint16_t)(20 + 6 * ta.attackPower) * attacker.healthPoints * pgm_read_byte(factor + stars);
  uint8_t dmg = (num + 2500) / 5000;
  return dmg ? dmg : 1;
}

bool AWGame::canCounter(const GameUnit &attacker, const GameUnit &defender){
  UnitTraits td = UnitTraits::traitsForUnitType(defender.type());
  return td.attackRange == 1 && manhattan(attacker.pos(), defender.pos()) == 1 &&
         GameUnit::canUnitAttackUnit(defender.type(), attacker.type());
}

bool AWGame::attackUnit(AWPlayer &ap, uint8_t ai, AWPlayer &dp, uint8_t di){
  GameUnit &a = ap.units[ai];
  GameUnit &d = dp.units[di];
  GameUnit a0 = a, d0 = d;   // as they were, for the battle screen
  uint8_t dmg = calculateDamage(a, d);
  d.healthPoints = (d.healthPoints > dmg) ? d.healthPoints - dmg : 0;
  uint8_t back = 0;
  if (d.healthPoints && canCounter(a, d)) {
    back = calculateDamage(d, a);
    a.healthPoints = (a.healthPoints > back) ? a.healthPoints - back : 0;
  }
  showBattle(a0, d0, ap.idx, dmg, back);
  bool survived = a.healthPoints > 0;
  if (!d.healthPoints) removeUnit(dp, di);
  if (!survived) removeUnit(ap, ai);
  updateMapForPlayer(viewer);
  return survived;
}

bool AWGame::canCaptureAt(const AWPlayer &p, UnitType t, Point pos){
  if (!unitIsInfantry(t)) return false;
  int8_t bi = buildingIndexAt(pos);
  if (bi < 0) return false;
  GameBuilding &b = gameBuildings[bi];
  return !(b.isOccupied && b.belongsToPlayer == p.idx);
}

bool AWGame::captureBuilding(AWPlayer &p, uint8_t ui){
  GameUnit &u = p.units[ui];
  int8_t bi = buildingIndexAt(u.pos());
  if (bi < 0) return false;
  GameBuilding &b = gameBuildings[bi];
  if (u.healthPoints >= b.capturePoints()) {
    b.isOccupied = 1;
    b.belongsToPlayer = p.idx;
    b.setCapturePoints(GameBuilding::FullCapturePoints);
    return true;
  }
  b.setCapturePoints(b.capturePoints() - u.healthPoints);
  return false;
}

// A half-captured building recovers when no capturing infantry stands on it any more.
void AWGame::resetAbandonedCaptures(){
  for (uint8_t i = 0; i < gameBuildings.getCount(); i++) {
    GameBuilding &b = gameBuildings[i];
    if (b.capturePoints() == GameBuilding::FullCapturePoints) continue;
    bool kept = false;
    for (uint8_t pi = 0; pi < 2; pi++) {
      int8_t ui = players[pi].unitIndexAt({b.mapPosX, b.mapPosY});
      if (ui >= 0 && unitIsInfantry(players[pi].units[ui].type()) && !(b.isOccupied && b.belongsToPlayer == pi))
        kept = true;
    }
    if (!kept) b.setCapturePoints(GameBuilding::FullCapturePoints);
  }
}

void AWGame::clearMarks(){
  for (uint16_t i = 0; i < (uint16_t)(mapSize.x * mapSize.y); i++) mapTileData[i].showSelection = 0;
}

void AWGame::restoreUnitSprites(){
  for (uint8_t pi = 0; pi < 2; pi++)
    for (uint8_t i = 0; i < players[pi].units.getCount(); i++) {
      GameUnit &u = players[pi].units[i];
      if (!u.loaded) tileAt(u.pos()).unitSpriteID = u.unitType;
    }
}

// Which tiles can a unit reach? The 4-bit unitSpriteID of each tile holds the
// best "movement left + 1" found so far; sweeps over the map spread it to the
// neighbours until nothing improves. No recursion, so no stack worries.
// Unit sprites are restored afterwards.
void AWGame::floodMove(int8_t x0, int8_t y0, uint8_t rem, UnitType t, uint8_t owner){
  uint16_t n = mapSize.x * mapSize.y;
  for (uint16_t i = 0; i < n; i++) mapTileData[i].unitSpriteID = 0;
  mapTileData[y0 * mapSize.x + x0].unitSpriteID = rem + 1;
  bool changed = true;
  while (changed) {
    changed = false;
    for (uint16_t i = 0; i < n; i++) {
      uint8_t left = mapTileData[i].unitSpriteID;
      if (left < 2) continue; // unreached, or no movement left
      int8_t x = i % mapSize.x, y = i / mapSize.x;
      for (uint8_t d = 0; d < 4; d++) {
        int8_t nx = x + (int8_t)pgm_read_byte(dirX + d);
        int8_t ny = y + (int8_t)pgm_read_byte(dirY + d);
        if (!inMap(nx, ny)) continue;
        MapTile &nt = mapTileData[ny * mapSize.x + nx];
        uint8_t c = moveCost(nt.type(), t);
        if (!c || c >= left) continue;
        if (nt.hasUnit && nt.unitBelongsTo != owner) continue; // enemies block the way
        if (nt.unitSpriteID < left - c) { nt.unitSpriteID = left - c; changed = true; }
      }
    }
  }
  for (uint16_t i = 0; i < n; i++) if (mapTileData[i].unitSpriteID) mapTileData[i].showSelection = 1;
}

void AWGame::markReachable(AWPlayer &p, uint8_t ui){
  GameUnit u = p.units[ui];
  clearMarks();
  UnitTraits tr = UnitTraits::traitsForUnitType(u.type());
  floodMove(u.mapPosX, u.mapPosY, tr.moveDistance, u.type(), p.idx);
  // you can pass your own units but not stop on them - except to board a transport
  for (uint8_t i = 0; i < p.units.getCount(); i++) {
    GameUnit &o = p.units[i];
    if (i == ui || o.loaded) continue;
    MapTile &t = tileAt(o.pos());
    if (!(unitIsInfantry(u.type()) && unitIsTransport(o.type()) && !o.carrying && !u.carrying))
      t.showSelection = 0;
  }
  restoreUnitSprites();
}

uint8_t AWGame::markTargets(AWPlayer &p, const GameUnit &u, Point from, Point origin, bool ignoreFog){
  UnitTraits tr = UnitTraits::traitsForUnitType(u.type());
  if (tr.attackPower == 0) return 0;
  bool indirect = tr.attackRange > 1;
  if (indirect && !(from == origin)) return 0; // indirect fire: move OR shoot
  uint8_t minR = indirect ? 2 : 1;
  uint8_t n = 0;
  AWPlayer &enemy = players[p.idx ^ 1];
  for (uint8_t i = 0; i < enemy.units.getCount(); i++) {
    GameUnit &e = enemy.units[i];
    if (e.loaded) continue;
    uint8_t d = manhattan(from, e.pos());
    if (d < minR || d > tr.attackRange) continue;
    if (!GameUnit::canUnitAttackUnit(u.type(), e.type())) continue;
    MapTile &t = tileAt(e.pos());
    if (t.showsFog && !ignoreFog) continue;
    t.showSelection = 1;
    n++;
  }
  return n;
}

uint8_t AWGame::markDropTiles(UnitType cargo, Point from){
  uint8_t n = 0;
  for (uint8_t d = 0; d < 4; d++) {
    int8_t nx = from.x + (int8_t)pgm_read_byte(dirX + d);
    int8_t ny = from.y + (int8_t)pgm_read_byte(dirY + d);
    if (!inMap(nx, ny)) continue;
    MapTile &t = mapTileData[ny * mapSize.x + nx];
    if (t.hasUnit || !moveCost(t.type(), cargo)) continue;
    t.showSelection = 1;
    n++;
  }
  return n;
}

// =====================================================================
// Map

void AWGame::loadMap(uint24_t mapAddress){
  uint8_t header[10];
  FX::readDataBytes(mapAddress, header, 10);
  mapSize.x = header[0];
  mapSize.y = header[1];
  gameBuildings.clear();

  uint16_t mapLength = mapSize.x * mapSize.y;
  FX::seekData(mapAddress + 10);
  for (uint16_t i = 0; i < mapLength; i++) {
    MapTile &tile = mapTileData[i];
    *reinterpret_cast<uint16_t *>(&tile) = 0;
    tile.tileID = (i + 1 < mapLength) ? FX::readPendingUInt8() : FX::readPendingLastUInt8();
    tile.unitIsActive = 1;

    MapTileType tileType = tile.type();
    if (!mapTileIndexIsBuilding(tileType)) continue;
    GameBuilding building;
    building.mapPosX = i % mapSize.x;
    building.mapPosY = i / mapSize.x;
    building.buildingType = tile.tileID;
    for (uint8_t pi = 0; pi < 2; pi++) {
      // header: P1 city, P1 factory, P2 city, P2 factory (x, y each)
      uint8_t *h = header + 2 + pi * 4;
      if ((tileType == MapTileType::City    && building.mapPosX == h[0] && building.mapPosY == h[1]) ||
          (tileType == MapTileType::Factory && building.mapPosX == h[2] && building.mapPosY == h[3]) ||
          (tileType == (pi ? MapTileType::P2HQ : MapTileType::P1HQ))) {
        building.isOccupied = 1;
        building.belongsToPlayer = pi;
      }
    }
    if (mapTileIsHQ(tileType)) players[tileType == MapTileType::P2HQ].cursorIndex = {building.mapPosX, building.mapPosY};
    gameBuildings.add(building);
  }
}

void AWGame::clearMap(bool withFog){
  for (uint16_t i = 0; i < (uint16_t)(mapSize.x * mapSize.y); i++) {
    MapTile &tile = mapTileData[i];
    tile.showsFog = withFog ? 1 : 0;
    tile.showSelection = 0;
    tile.hasUnit = 0;
    tile.unitBelongsTo = 0;
    tile.unitSpriteID = 0;
    tile.unitIsActive = 1;
  }
}

// Rebuilds the per-tile flags (units, fog, building owners) as seen by one player.
void AWGame::updateMapForPlayer(uint8_t viewerIdx){
  AWPlayer &me = players[viewerIdx];
  // a unit parked on a science lab gives satellite pictures: no fog
  bool satellite = false;
  for (uint8_t i = 0; i < me.units.getCount(); i++)
    if (!me.units[i].loaded && tileAt(me.units[i].pos()).type() == MapTileType::ScienceLab) satellite = true;

  clearMap(!satellite);

  for (uint8_t pi = 0; pi < 2; pi++) {
    AWPlayer &p = players[pi];
    for (uint8_t i = 0; i < p.units.getCount(); i++) {
      GameUnit &u = p.units[i];
      if (u.loaded) continue;
      MapTile &tile = tileAt(u.pos());
      tile.hasUnit = 1;
      tile.unitBelongsTo = pi;
      tile.unitSpriteID = u.unitType;
      tile.unitIsActive = (pi == viewerIdx) ? u.activated : 1;
    }
  }

  if (!satellite) {
    for (uint8_t i = 0; i < me.units.getCount(); i++) {
      GameUnit &u = me.units[i];
      if (u.loaded) continue;
      uint8_t sight = UnitTraits::traitsForUnitType(u.type()).moveDistance;
      if (sight > 4) sight = 4;
      if (tileAt(u.pos()).type() == MapTileType::Mountain) sight += 2;
      removeFogAtPositionRadius(u.pos(), sight, viewerIdx, false);
    }
  }

  for (uint8_t i = 0; i < gameBuildings.getCount(); i++) {
    GameBuilding &b = gameBuildings[i];
    MapTile &tile = tileAt({b.mapPosX, b.mapPosY});
    tile.buildingIsOccupied = b.isOccupied;
    tile.buildingBelongsTo = (b.belongsToPlayer == viewerIdx) ? MapTile::BelongsToPlayer : MapTile::BelongsToEnemy;
    if (b.isOccupied && b.belongsToPlayer == viewerIdx && !satellite)
      removeFogAtPositionRadius({b.mapPosX, b.mapPosY}, GameBuilding::buildingViewDistance, viewerIdx, true);
  }
}

void AWGame::removeFogAtPositionRadius(Point origin, uint8_t radius, uint8_t viewerIdx, bool seeThrough){
  // Bresenham circle, a line of sight ray to every point on it
  int8_t r = radius * 2;
  int8_t x = -r, y = 0, err = 2 - 2 * r;
  do {
    castRayTo(origin, seeThrough, viewerIdx, origin.x - x, origin.y + y);
    castRayTo(origin, seeThrough, viewerIdx, origin.x - y, origin.y - x);
    castRayTo(origin, seeThrough, viewerIdx, origin.x + x, origin.y - y);
    castRayTo(origin, seeThrough, viewerIdx, origin.x + y, origin.y + x);
    r = err;
    if (r <= y) err += ++y * 2 + 1;
    if (r > x || err > y) err += ++x * 2 + 1;
  } while (x < 0);
}

void AWGame::castRayTo(Point origin, bool seeThrough, uint8_t viewerIdx, int8_t xEnd, int8_t yEnd){
  int8_t x0 = origin.x, y0 = origin.y;
  int8_t dx =  abs(xEnd - x0), sx = x0 < xEnd ? 1 : -1;
  int8_t dy = -abs(yEnd - y0), sy = y0 < yEnd ? 1 : -1;
  int8_t err = dx + dy, e2;
  int8_t forests = 2;            // you can look through two forests
  int8_t r = (dx - dy) / 2;
  while (true) {
    uint8_t dist = abs(origin.x - x0) + abs(origin.y - y0);
    if (!inMap(x0, y0) || dist > r) break;
    MapTile &tile = mapTileData[x0 + y0 * mapSize.x];
    tile.showsFog = 0;
    if (dist && !seeThrough) {
      if (mapTileIsOpaque(tile.type())) break;
      if (tile.hasUnit && tile.unitBelongsTo != viewerIdx) break;
      if (tile.type() == MapTileType::Forest && --forests < 0) break;
    }
    if (x0 == xEnd && y0 == yEnd) break;
    e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

Point AWGame::nextMarkedMapPosition(Point current, int8_t direction){
  int16_t n = mapSize.x * mapSize.y;
  int16_t i = current.y * mapSize.x + current.x;
  for (int16_t k = 1; k <= n; k++) {
    int16_t j = (i + direction * k + 2 * n) % n;
    if (mapTileData[j].showSelection) return {(int16_t)(j % mapSize.x), (int16_t)(j / mapSize.x)};
  }
  return {-1, -1};
}

// =====================================================================
// Drawing

Point AWGame::calculateCameraPosition(Point c){
  Point cam;
  cam.x = c.x - (WIDTH - 32) / 2;
  cam.y = c.y - (HEIGHT - 32) / 2;
  Point px = mapSize * TILE_SIZE;
  if (cam.x > px.x - WIDTH) cam.x = px.x - WIDTH;
  if (cam.y > px.y - HEIGHT + mapOffsetY) cam.y = px.y - HEIGHT + mapOffsetY;
  if (cam.x < 0) cam.x = 0;
  if (cam.y < 1) cam.y = 1;
  return cam;
}

void AWGame::drawMapAtPosition(Point pos){
  for (int8_t y = 0; y < mapSize.y; y++) {
    int16_t py = pos.y + y * TILE_SIZE;
    if (py <= -TILE_SIZE || py >= HEIGHT) continue;
    for (int8_t x = mapSize.x - 1; x >= 0; x--) { // right to left: markers overlap the tile to the right
      int16_t px = pos.x + x * TILE_SIZE;
      if (px <= -TILE_SIZE || px >= WIDTH) continue;
      const MapTile tile = mapTileData[y * mapSize.x + x];
      const MapTileType tileType = tile.type();

      FX::drawBitmap(px, py, worldTiles, tile.tileID, dbmNormal);
      // the HQ is two tiles tall: its roof is drawn over the tile above
      if (y < mapSize.y - 1) {
        MapTileType below = mapTileData[(y + 1) * mapSize.x + x].type();
        if (mapTileIsHQ(below)) FX::drawBitmap(px, py, worldTiles, (below == MapTileType::P1HQ) ? 32 : 33, dbmNormal);
      }

      if (tile.showsFog && !tile.showSelection) {
        FX::drawBitmap(px, py, fogGfx, 0, dbmBlack);
      }
      else if (mapTileIndexIsBuilding(tileType) && !mapTileIsHQ(tileType) && tile.buildingIsOccupied) {
        FX::drawBitmap(px + 10, py - 2, markerGfx, tile.buildingBelongsTo, dbmMasked);
      }

      if (tile.showSelection) FX::drawBitmap(px, py, selectGfx, (arduboy.frameCount / 10) % 4, dbmBlack);

      if (tile.hasUnit && !tile.showsFog) {
        uint24_t sheet = tile.unitBelongsTo ? unitsB : unitsA;
        if (tile.unitIsActive) {
          FX::drawBitmap(px, py, sheet, tile.unitSpriteID * 2 + (arduboy.frameCount / 10) % 2, dbmMasked);
        }
        else {
          FX::drawBitmap(px, py, sheet, tile.unitSpriteID * 2, dbmMasked);
          FX::drawBitmap(px + 10, py - 2, markerGfx, 2, dbmMasked);
        }
      }
    }
  }
}

// Damaged units show their health in the corner, like the original.
void AWGame::drawUnitHealth(){
  tinyfont.setTextColor(WHITE);
  for (uint8_t pi = 0; pi < 2; pi++) {
    for (uint8_t i = 0; i < players[pi].units.getCount(); i++) {
      GameUnit &u = players[pi].units[i];
      if (u.loaded || u.healthPoints >= GameUnit::UnitDefaultHealth) continue;
      if (tileAt(u.pos()).showsFog) continue;
      int16_t x = u.mapPosX * TILE_SIZE - camera.x + 11;
      int16_t y = u.mapPosY * TILE_SIZE - camera.y + 10;
      if (x < -5 || x >= WIDTH || y < 7 || y >= HEIGHT) continue;
      fillRect(x, y, 6, 6, BLACK);
      tinyfont.setCursor(x + 1, y + 1);
      printNumber(u.healthPoints);
    }
  }
  tinyfont.setTextColor(BLACK);
}

void AWGame::drawHud(uint8_t playerIdx, Point tile){
  fillRect(0, 0, 128, 7, BLACK);
  fillRect(0, 0, 128, 6, WHITE);
  tinyfont.setCursor(1, 1);
  MapTile t = tileAt(tile);
  int8_t ui = -1;
  AWPlayer *owner = nullptr;
  if (inMap(tile.x, tile.y) && t.hasUnit && !t.showsFog) {
    owner = &players[t.unitBelongsTo];
    ui = owner->unitIndexAt(tile);
  }
  if (ui >= 0) {
    GameUnit &u = owner->units[ui];
    tinyfont.print(FS(pgm_read_word(&LOCA_Unit_Names[u.unitType])));
    if (u.carrying) tinyfont.print('+');
    tinyfont.setCursor(56, 1);
    tinyfont.print(F("HP"));
    printNumber(u.healthPoints);
  }
  else {
    if (vsComputer) tinyfont.print(FS(playerIdx ? LOCA_enemyTurn : LOCA_you));
    else tinyfont.print(FS(playerIdx ? LOCA_player2 : LOCA_player1));
    tinyfont.setCursor(56, 1);
    tinyfont.print(FS(LOCA_day));
    printNumber(daysPlayed);
  }
  tinyfont.setCursor(86, 1);
  tinyfont.print(FS(LOCA_funds));
  printNumber(players[viewer].money);
  if (players[viewer].money) tinyfont.print(F("00"));
}

void AWGame::renderScene(Point cursor, bool showCursor, int8_t ghostType, uint8_t hudPlayer){
  camera = calculateCameraPosition(cursor);
  camera.y -= mapOffsetY;
  arduboy.clear();
  drawMapAtPosition(camera * -1);
  drawUnitHealth();
  if (showCursor)
    FX::drawBitmap(cursor.x - camera.x, cursor.y - camera.y + 1, cursorGfx, (arduboy.frameCount / 30) % 2, dbmMasked);
  if (ghostType >= 0)
    FX::drawBitmap(cursor.x - camera.x + 8, cursor.y - camera.y + 8, hudPlayer ? unitsB : unitsA,
                   ghostType * 2 + (arduboy.frameCount / 10) % 2, dbmMasked);
  Point tile = cursorToTile(cursor);
  drawHud(hudPlayer, tile);
}

void AWGame::showScene(Point cursor, uint8_t frames){
  while (frames) {
    if (!nextFrame()) continue;
    renderScene(cursor, true, -1, 1);
    FX::display();
    frames--;
  }
}

bool AWGame::nextFrame(){
  if (!arduboy.nextFrame()) return false;
  beep.timer();
  return true;
}

void AWGame::sfx(uint16_t count, uint8_t dur){
  beep.tone(count, dur);
}

// Byte-wise rectangle fill. Arduboy2's fillRect sets one pixel at a time,
// which was a third of the frame time on the map screen.
void AWGame::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color){
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > WIDTH)  w = WIDTH - x;
  if (y + h > HEIGHT) h = HEIGHT - y;
  if (w <= 0 || h <= 0) return;
  uint8_t *buf = arduboy.getBuffer();
  int16_t end = y + h;
  while (y < end) {
    uint8_t bit = y & 7;
    uint8_t n = 8 - bit;
    if (n > end - y) n = end - y;
    uint8_t mask = (uint8_t)(((1 << n) - 1) << bit);
    uint8_t *p = buf + (y >> 3) * WIDTH + x;
    for (int16_t i = 0; i < w; i++) p[i] = color ? (p[i] | mask) : (p[i] & ~mask);
    y += n;
  }
}

// A white box with a black frame, a thicker bottom edge and clipped corners.
void AWGame::drawBox(int16_t x, int16_t y, uint8_t w, uint8_t h){
  fillRect(x, y, w, h, BLACK);
  fillRect(x + 1, y + 1, w - 2, h - 3, WHITE);
  arduboy.drawPixel(x + 1, y + 1, BLACK);
  arduboy.drawPixel(x + w - 2, y + 1, BLACK);
  arduboy.drawPixel(x + 1, y + h - 3, BLACK);
  arduboy.drawPixel(x + w - 2, y + h - 3, BLACK);
}

// Prints a number without the 33 byte buffer Print::print(long) puts on the stack.
void AWGame::printNumber(uint16_t v){
  char buf[6];
  uint8_t i = sizeof(buf);
  buf[--i] = 0;
  do { buf[--i] = '0' + v % 10; v /= 10; } while (v);
  tinyfont.print(buf + i);
}

uint8_t AWGame::rnd(){
  // xorshift16
  rngState ^= rngState << 7;
  rngState ^= rngState >> 9;
  rngState ^= rngState << 8;
  return rngState;
}

void AWGame::showDialog(char_P *titleText){
  uint8_t w = strlen_P(titleText) * 5 + 8;
  uint8_t x = (WIDTH - w) / 2;
  arduboy.pollButtons();
  while (true) {
    if(!nextFrame()) continue;
    arduboy.pollButtons();
    if (arduboy.justPressed(B_BUTTON)) return;
    drawBox(x, 24, w, 16);
    tinyfont.setCursor(x + 4, 30);
    tinyfont.print(FS(titleText));
    FX::display();
  }
}

void AWGame::showBanner(char_P *titleText, uint8_t frames){
  uint8_t w = strlen_P(titleText) * 5 + 8;
  uint8_t x = (WIDTH - w) / 2;
  while (frames) {
    if(!nextFrame()) continue;
    drawBox(x, 24, w, 16);
    tinyfont.setCursor(x + 4, 30);
    tinyfont.print(FS(titleText));
    FX::display();
    frames--;
  }
}

int8_t AWGame::showOptions(char_P *options[], uint8_t count){
  constexpr uint8_t w = 44;
  uint8_t h = 6 + count * 7;
  uint8_t x = WIDTH - w - 4, y = 10;
  int8_t selectedIndex = 0;
  while (true) {
    if(!nextFrame()) continue;
    arduboy.pollButtons();
    if (arduboy.justPressed(A_BUTTON)) return -1;
    if (arduboy.justPressed(B_BUTTON)) { sfx(BeepMenu, 3); return selectedIndex; }
    if (arduboy.justPressed(DOWN_BUTTON)) selectedIndex = (selectedIndex + 1) % count;
    if (arduboy.justPressed(UP_BUTTON))   selectedIndex = (selectedIndex + count - 1) % count;
    drawBox(x, y, w, h);
    for (uint8_t i = 0; i < count; i++) {
      tinyfont.setCursor(x + 9, y + 4 + 7 * i);
      tinyfont.print(FS(options[i]));
    }
    tinyfont.setCursor(x + 4, y + 4 + 7 * selectedIndex);
    tinyfont.print(FS(LOCA_Cursor));
    FX::display();
  }
}

UnitType AWGame::showShopForBuildingAndPlayer(MapTileType building, AWPlayer &aPlayer){
  int8_t idx = 0;
  uint8_t numberOfUnits;
  const UnitType *buyable;
  switch (building) {
    case MapTileType::Factory:  numberOfUnits = NumberOfBuyableUnitsAtFactory;  buyable = buyableUnitsAtFactory;  break;
    case MapTileType::Airport:  numberOfUnits = NumberOfBuyableUnitsAtAirport;  buyable = buyableUnitsAtAirPort;  break;
    case MapTileType::Shipyard: numberOfUnits = NumberOfBuyableUnitsAtShipyard; buyable = buyableUnitsAtShipyard; break;
    default: return UnitType::None;
  }
  constexpr uint8_t pad = 8;
  while (true) {
    if(!nextFrame()) continue;
    arduboy.pollButtons();
    if (arduboy.justPressed(DOWN_BUTTON) && idx < numberOfUnits - 1) idx++;
    if (arduboy.justPressed(UP_BUTTON) && idx > 0) idx--;
    if (arduboy.justPressed(A_BUTTON)) return UnitType::None;
    UnitType current = static_cast<UnitType>(pgm_read_byte(buyable + idx));
    if (arduboy.justPressed(B_BUTTON) && GameUnit::costsOfUnit(current) <= aPlayer.money) return current;

    int8_t yOffset = min((4 - idx) * pad, 0);
    drawBox(3, 9, WIDTH - 6, HEIGHT - 9);
    for (uint8_t i = 0; i < numberOfUnits; i++) {
      UnitType t = static_cast<UnitType>(pgm_read_byte(buyable + i));
      bool afford = GameUnit::costsOfUnit(t) <= aPlayer.money;
      int8_t yPos = 14 + i * pad + yOffset;
      if (yPos < 10 || yPos > HEIGHT - 8) continue;
      tinyfont.setCursor(12 + ((idx == i && afford) ? 4 : 0), yPos);
      tinyfont.print(FS(pgm_read_word(&LOCA_Unit_Names[static_cast<uint8_t>(t)])));
      if (!afford) { tinyfont.setCursor(62, yPos); tinyfont.print('X'); }
    }
    tinyfont.setCursor(6, 14 + idx * pad + yOffset);
    tinyfont.print(FS(LOCA_Cursor));

    FX::drawBitmap(92, 11, aPlayer.idx ? unitsB : unitsA, static_cast<uint8_t>(current) * 2 + (arduboy.frameCount / 10) % 2, dbmMasked);
    UnitTraits tr = UnitTraits::traitsForUnitType(current);
    char_P *labels[4] = {LOCA_Trait_attack, LOCA_Trait_defense, LOCA_Trait_speed, LOCA_Trait_range};
    uint8_t values[4] = {tr.attackPower, tr.defense, tr.moveDistance, tr.attackRange};
    tinyfont.setCursor(72, 29);
    printNumber(GameUnit::costsOfUnit(current));
    tinyfont.print(F("00"));
    for (uint8_t i = 0; i < 4; i++) {
      tinyfont.setCursor(72, 36 + i * 6);
      tinyfont.print(FS(labels[i]));
      tinyfont.setCursor(112, 36 + i * 6);
      printNumber(values[i]);
    }
    FX::display();
  }
}

// Slides a unit to `to` along its shortest path, a tile at a time, with the
// camera following. Like Advance Wars, an enemy is only drawn on tiles you can
// see: it vanishes into the fog and reappears, and a move entirely inside the
// fog isn't shown. The caller moves the unit for real afterwards.
void AWGame::animateMove(AWPlayer &p, uint8_t ui, Point to){
  GameUnit u = p.units[ui];
  Point from = u.pos();
  if (from == to) return;
  floodMove(u.mapPosX, u.mapPosY, UnitTraits::traitsForUnitType(u.type()).moveDistance, u.type(), p.idx);
  // walk back from the destination: the tile we came from is the neighbour
  // that had exactly this tile's cost more movement left
  uint8_t steps[8];
  uint8_t n = 0;
  bool own = p.idx == viewer, seen = own || !tileAt(to).showsFog;
  Point c = to;
  while (!(c == from) && n < 8) {
    uint8_t need = tileAt(c).unitSpriteID + moveCost(tileAt(c).type(), u.type());
    uint8_t d = 0;
    for (; d < 4; d++) {
      int8_t nx = c.x - (int8_t)pgm_read_byte(dirX + d), ny = c.y - (int8_t)pgm_read_byte(dirY + d);
      if (inMap(nx, ny) && mapTileData[ny * mapSize.x + nx].unitSpriteID == need) { c = {nx, ny}; break; }
    }
    if (d == 4) break;           // can't happen, but never loop forever
    steps[n++] = d;
    if (!tileAt(c).showsFog) seen = true;
  }
  restoreUnitSprites();
  clearMarks();
  if (!seen) return;
  tileAt(from).hasUnit = 0;      // drawn by hand while it moves
  Point px = from * TILE_SIZE;
  while (n) {
    uint8_t d = steps[--n];
    for (uint8_t k = 0; k < TILE_SIZE / 2; ) {
      if (!nextFrame()) continue;
      px.x += 2 * (int8_t)pgm_read_byte(dirX + d);
      px.y += 2 * (int8_t)pgm_read_byte(dirY + d);
      k++;
      bool show = own || !tileAt({(int16_t)((px.x + 8) / TILE_SIZE), (int16_t)((px.y + 8) / TILE_SIZE)}).showsFog;
      renderScene({(int16_t)(px.x - 8), (int16_t)(px.y - 8)}, false, show ? (int8_t)u.unitType : -1, p.idx);
      FX::display();
    }
  }
}

// The battle screen, as in Advance Wars: player 1's squad on the left facing
// right, player 2's on the right facing left, one figure per 2 HP, each on the
// terrain it stands on. The attacker fires, explosions land on the defender as
// its HP counts down and figures drop out, then the counter-attack (if any).
// B skips. a and d are the units before the fight.
static const uint8_t PROGMEM squadX[5] = {4, 24, 44, 14, 34};
static const uint8_t PROGMEM squadY[5] = {22, 22, 22, 33, 33};

void AWGame::showBattle(GameUnit a, GameUnit d, uint8_t aOwner, uint8_t dmg, uint8_t back){
#ifdef AW_AUTOPLAY
  return;   // computer vs computer test runs: rules only
#endif
  GameUnit side[2];
  side[aOwner] = a;
  side[aOwner ^ 1] = d;
  uint8_t s = aOwner;            // the side that fires
  bool skip = false;
  for (uint8_t round = 0; round < 2 && dmg; round++) {
    uint8_t t = s ^ 1;
    uint8_t goal = side[t].healthPoints > dmg ? side[t].healthPoints - dmg : 0;
    uint8_t n0 = (side[t].healthPoints + 1) / 2;   // the target's figures before the hit
    for (uint8_t f = 0; f < 96; ) {
      if (!nextFrame()) continue;
      arduboy.pollButtons();
      if (arduboy.justPressed(B_BUTTON)) skip = true;
      if (skip) { side[t].healthPoints = goal; f = 95; }
      if (f == 6) sfx(BeepMenu, 4);
      if (f >= 30 && !(f & 3) && side[t].healthPoints > goal) {
        if (--side[t].healthPoints == goal) sfx(goal ? BeepHit : BeepKill, goal ? 10 : 30);
      }
      fillRect(0, 0, 128, 48, WHITE);
      for (uint8_t k = 0; k < 2; k++) {
        GameUnit &u = side[k];
        uint8_t tid = tileAt(u.pos()).tileID;
        for (uint8_t i = 0; i < 4; i++) FX::drawBitmap(k * 64 + i * 16, 48, worldTiles, tid, dbmNormal);
        int8_t lift = (moveClassOf(u.type()) == MoveClass::Air) ? 14 : 0;   // aircraft fly
        uint8_t n = (u.healthPoints + 1) / 2;
        for (uint8_t j = 0; j < 5; j++) {
          int16_t x = pgm_read_byte(squadX + j), y = pgm_read_byte(squadY + j) - lift;
          if (k) x = 112 - x;    // mirrored formation on the right
          if (j < n) {
            bool firing = k == s && f < 40;
            FX::drawBitmap(x, y, k ? unitsB : unitsA, u.unitType * 2 + (firing ? (f / 6) & 1 : 0), dbmMasked);
            if (firing && f >= 6 && f < 30) {
              if (f & 4) FX::drawBitmap(k ? x - 8 : x + 16, y + 4, flashGfx, (f >> 3) & 1, dbmMasked);
              fillRect(k ? x - 6 - (f - 6) * 4 : x + 18 + (f - 6) * 4, y + 7, 3, 1, BLACK);   // tracer
            }
          }
          if (k == t && f >= 22 && f < 78 && j == ((f - 22) >> 3) % n0)
            FX::drawBitmap(x, y, boomGfx, ((f - 22) & 7) / 3, dbmMasked);
        }
        tinyfont.setCursor(1 + k * 65, 1);
        tinyfont.print(FS(pgm_read_word(&LOCA_Unit_Names[u.unitType])));
        tinyfont.setCursor(52 + k * 65, 1);
        printNumber(u.healthPoints);
      }
      fillRect(0, 6, 128, 1, BLACK);
      fillRect(0, 47, 128, 1, BLACK);   // horizon
      fillRect(63, 6, 2, 58, BLACK);
      FX::display();
      f++;
    }
    s = t;
    dmg = back;
  }
}

void AWGame::makeScreenTransition(){
  uint8_t *dbuff = arduboy.getBuffer();
  for (uint8_t i = 0; i < WIDTH / 2; i++) {
    for (uint8_t y = 0; y < 8; y++) {
      uint8_t *row = dbuff + y * WIDTH;
      if (y % 2 == 0) { memmove(row, row + 2, WIDTH - 2); row[WIDTH - 2] = row[WIDTH - 1] = 0; }
      else            { memmove(row + 2, row, WIDTH - 2); row[0] = row[1] = 0; }
    }
    FX::display();
    delay(2);
  }
}
