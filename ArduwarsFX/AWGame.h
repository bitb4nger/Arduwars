#ifndef GAME_H
#define GAME_H

#include <Arduboy2.h>
#include <ArduboyFX.h>
#include <Arduboy2Beep.h>

#include "DataClasses.h"
#include "fxdata/fxdata.h"

using char_P = const char;

// Text is drawn with the 4x4 Tinyfont glyphs stored on the FX chip.
// Same calls as the Tinyfont library the original used.
struct FxText {
  void setCursor(int16_t x, int16_t y){ FX::setCursor(x, y); }
  void setTextColor(uint8_t c){ FX::setFont(c ? fontWhite : fontBlack, dcmMasked); }
  void print(char c){ FX::drawChar(c); }
  void print(const char *s){ while (*s) FX::drawChar(*s++); }
  void print(const __FlashStringHelper *f){
    const char *s = reinterpret_cast<const char *>(f);
    char c;
    while ((c = pgm_read_byte(s++))) FX::drawChar(c);
  }
};

enum class AWGameState : uint8_t {
  showMainMenu,
  continueGame,
  playSinglePlayer,
  playMultiPlayer
};

// What the player is doing during their turn.
enum class AWTurnState : uint8_t {
  Default,       // moving the cursor around
  UnitSelected,  // a unit is picked up, its range is shown
  UnitAttack,    // choosing a target
  UnitDrop       // choosing where a transport drops its passenger
};

constexpr uint8_t TILE_SIZE = 16;

// what the computer decided for one unit: where to go, then 0 = wait, 1 = attack, 2 = capture
struct AIPlan { Point dest; uint8_t action; int8_t target; };
constexpr uint16_t MAX_MAP_TILES = 384; // 24x16, the biggest map

class AWGame{
  public:
    AWGame(void) = default;
    void run(void);

  private:
    // ---------------- menus and game flow
    __attribute__((noinline)) AWGameState showMainMenu();
    __attribute__((noinline)) int8_t showMapSelection();          // map index or -1
    __attribute__((noinline)) void playGame(bool resume);
    void saveGame(){ FX::saveGameState(&saveMagic, (const uint8_t *)(&gameBuildings + 1) - &saveMagic); }
    bool loadSave(){ return FX::loadGameState(&saveMagic, (const uint8_t *)(&gameBuildings + 1) - &saveMagic) && saveMagic == SaveMagic; }
    __attribute__((noinline)) void beginTurn(AWPlayer &p);
    __attribute__((noinline)) bool doHumanTurn(AWPlayer &p);       // false = player quit
    __attribute__((noinline)) void doComputerTurn(AWPlayer &p);
    __attribute__((noinline)) uint8_t checkWinner();               // 0 = nobody yet, 1 = P1, 2 = P2
    __attribute__((noinline)) void showGameOver(uint8_t winner);
    enum : uint8_t { ActFire, ActCapture, ActLoad, ActDrop, ActWait, ActCancel };
    __attribute__((noinline)) uint8_t chooseUnitAction(AWPlayer &p, int8_t sel, Point origin, Point dest);
    __attribute__((noinline)) int8_t turnMenu();
    __attribute__((noinline)) uint8_t humanPressB(AWPlayer &p);
    // the human's turn in progress
    Point hCursor, hOrigin, hDest;
    int8_t hSel;
    AWTurnState hState;
    __attribute__((noinline)) void drawMapAtPosition(Point pos);
    __attribute__((noinline)) void drawHud(uint8_t playerIdx, Point tile);

    // ---------------- rules
    uint8_t calculateDamage(const GameUnit &attacker, const GameUnit &defender);
    bool canCounter(const GameUnit &attacker, const GameUnit &defender);
    __attribute__((noinline)) bool attackUnit(AWPlayer &ap, uint8_t ai, AWPlayer &dp, uint8_t di); // true if attacker survived
    __attribute__((noinline)) bool captureBuilding(AWPlayer &p, uint8_t ui);                         // true if it changed owner
    bool canCaptureAt(const AWPlayer &p, UnitType t, Point pos);
    __attribute__((noinline)) void moveUnitTo(AWPlayer &p, uint8_t ui, Point to);
    __attribute__((noinline)) void animateMove(AWPlayer &p, uint8_t ui, Point to);   // slide along the path
    __attribute__((noinline)) void showBattle(GameUnit a, GameUnit d, uint8_t aOwner, uint8_t dmg, uint8_t back);
    void removeUnit(AWPlayer &p, uint8_t ui);
    int8_t passengerOf(AWPlayer &p, uint8_t ui);
    __attribute__((noinline)) void resetAbandonedCaptures();
    int8_t buildingIndexAt(Point pos);
    __attribute__((noinline)) uint8_t markTargets(AWPlayer &p, const GameUnit &u, Point from, Point origin, bool ignoreFog);
    __attribute__((noinline)) uint8_t markDropTiles(UnitType cargo, Point from);
    __attribute__((noinline)) void markReachable(AWPlayer &p, uint8_t ui);
    __attribute__((noinline)) void floodMove(int8_t x, int8_t y, uint8_t rem, UnitType t, uint8_t owner);
    void restoreUnitSprites();
    void clearMarks();
    __attribute__((noinline)) bool buyUnit(AWPlayer &p, UnitType t, Point pos);

    // ---------------- map
    void loadMap(uint24_t mapAddress);
    MapTile & tileAt(Point p){ return mapTileData[p.y * mapSize.x + p.x]; }
    bool inMap(int8_t x, int8_t y){ return x >= 0 && y >= 0 && x < mapSize.x && y < mapSize.y; }
    __attribute__((noinline)) void updateMapForPlayer(uint8_t viewerIdx);
    void clearMap(bool withFog);
    void removeFogAtPositionRadius(Point origin, uint8_t radius, uint8_t viewerIdx, bool seeThrough);
    void castRayTo(Point origin, bool seeThrough, uint8_t viewerIdx, int8_t xEnd, int8_t yEnd);
    __attribute__((noinline)) Point nextMarkedMapPosition(Point current, int8_t direction);

    // ---------------- drawing
    void renderScene(Point cursor, bool showCursor, int8_t ghostType, uint8_t hudPlayer);
    void showScene(Point cursor, uint8_t frames);
    void drawUnitHealth();
    Point calculateCameraPosition(Point forCursorPosition);
    void drawBox(int16_t x, int16_t y, uint8_t w, uint8_t h);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
    uint8_t rnd();
    void printNumber(uint16_t v);
    uint16_t rngState = 1;
    void showDialog(char_P *titleText);
    void showBanner(char_P *titleText, uint8_t frames);
    __attribute__((noinline)) int8_t showOptions(char_P *options[], uint8_t count);
    __attribute__((noinline)) UnitType showShopForBuildingAndPlayer(MapTileType building, AWPlayer &aPlayer);
    void makeScreenTransition();
    static Point tileToCursor(Point t){ return {(int16_t)(t.x * TILE_SIZE - 8), (int16_t)(t.y * TILE_SIZE - 8)}; }
    static Point cursorToTile(Point c){ return {(int16_t)((c.x + TILE_SIZE) / TILE_SIZE), (int16_t)((c.y + TILE_SIZE) / TILE_SIZE)}; }

    // ---------------- computer player (AWAI.cpp)
    __attribute__((noinline)) AIPlan aiPlanUnit(AWPlayer &me, uint8_t ui);
    __attribute__((noinline)) void aiMoveUnit(AWPlayer &me, uint8_t ui, AIPlan plan);
    __attribute__((noinline)) void aiBuyUnits(AWPlayer &me);
    void aiDistanceField(AWPlayer &me, const GameUnit &u, bool hqOnly);
    int16_t aiAttackScore(const GameUnit &a, Point from, const GameUnit &d);
    void aiShowUnit(Point tile, uint8_t frames);

    // ---------------- sound: a few beeps on the speaker (muted by SOUND OFF)
    __attribute__((noinline)) bool nextFrame();
    __attribute__((noinline)) void sfx(uint16_t count, uint8_t dur);

    // ---------------- data
    Arduboy2Base arduboy;
    BeepPin1 beep;
    FxText tinyfont;

    AWGameState gameState = AWGameState::showMainMenu;
    uint8_t viewer = 0;   // whose eyes the map (fog, building flags) is drawn for
    bool hasSave = false;

    // ---- everything from saveMagic to gameBuildings is the autosave, keep them together
    static constexpr uint8_t SaveMagic = 0xA7;
    uint8_t saveMagic = 0;
    uint8_t mapIndex = 0;
    bool vsComputer = false;
    uint8_t daysPlayed;
    uint8_t turnPlayer;
    AWPlayer players[2] = {AWPlayer(0), AWPlayer(1)};
    FixedList<GameBuilding, 24> gameBuildings;
    // ---- end of autosave

    MapTile mapTileData[MAX_MAP_TILES];
    Point mapSize = {0, 0};
    Point camera = {0, 0};
    static constexpr uint8_t mapOffsetY = 8;
};
#endif
