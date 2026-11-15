#ifndef LOCA_H
#define LOCA_H

#include <avr/pgmspace.h>

// All texts of the game. Include this in one .cpp file only.
using char_P = const char;

char_P LOCA_SinglePlayer[] PROGMEM    = "VS COMPUTER";
char_P LOCA_MultiPlayer[] PROGMEM     = "2 PLAYERS";
char_P LOCA_Sound_on[] PROGMEM        = "SOUND ON";
char_P LOCA_Sound_off[] PROGMEM       = "SOUND OFF";
char_P LOCA_BackWithA[] PROGMEM       = "BACK WITH A";
char_P LOCA_Cursor[] PROGMEM          = ">";

char_P LOCA_mapSmall[] PROGMEM        = "SKIRMISH";
char_P LOCA_mapMedium[] PROGMEM       = "BATTLE";
char_P LOCA_mapBig[] PROGMEM          = "ARDUWARS";

char_P LOCA_player1[] PROGMEM         = "PLAYER 1";
char_P LOCA_player2[] PROGMEM         = "PLAYER 2";
char_P LOCA_you[] PROGMEM             = "YOU";
char_P LOCA_enemyTurn[] PROGMEM       = "ENEMY TURN";

char_P LOCA_endTurn[] PROGMEM         = "END TURN";
char_P LOCA_quit[] PROGMEM            = "QUIT";
char_P LOCA_day[] PROGMEM             = "DAY";
char_P LOCA_funds[] PROGMEM           = "M:";
char_P LOCA_Unit_limit_reached[] PROGMEM = "UNIT LIMIT REACHED";

char_P LOCA_attack[] PROGMEM          = "FIRE";
char_P LOCA_wait[] PROGMEM            = "WAIT";
char_P LOCA_cancel[] PROGMEM          = "CANCEL";
char_P LOCA_load[] PROGMEM            = "LOAD";
char_P LOCA_drop[] PROGMEM            = "DROP";
char_P LOCA_capture[] PROGMEM         = "CAPTURE";

char_P LOCA_Trait_attack[] PROGMEM    = "ATK";
char_P LOCA_Trait_defense[] PROGMEM   = "DEF";
char_P LOCA_Trait_speed[] PROGMEM     = "MOVE";
char_P LOCA_Trait_range[] PROGMEM     = "RNG";

char_P LOCA_youWin[] PROGMEM          = "VICTORY!";
char_P LOCA_youLose[] PROGMEM         = "DEFEAT";
char_P LOCA_p1Wins[] PROGMEM          = "PLAYER 1 WINS";
char_P LOCA_p2Wins[] PROGMEM          = "PLAYER 2 WINS";
char_P LOCA_draw[] PROGMEM            = "DRAW";
char_P LOCA_captured[] PROGMEM        = "CAPTURED!";

char_P LOCA_Unit_Soldier[] PROGMEM        = "SOLDIER";
char_P LOCA_Unit_Mech[] PROGMEM           = "MECH";
char_P LOCA_Unit_SpecOps[] PROGMEM        = "SPECOPS";
char_P LOCA_Unit_scout[] PROGMEM          = "SCOUT";
char_P LOCA_Unit_Assist[] PROGMEM         = "ASSIST";
char_P LOCA_Unit_Tank[] PROGMEM           = "TANK";
char_P LOCA_Unit_BigTank[] PROGMEM        = "BIGTANK";
char_P LOCA_Unit_Artillery[] PROGMEM      = "ARTILLERY";
char_P LOCA_Unit_Rocket[] PROGMEM         = "ROCKET";
char_P LOCA_Unit_Missiles[] PROGMEM       = "MISSILES";
char_P LOCA_Unit_Heli[] PROGMEM           = "HELI";
char_P LOCA_Unit_Fighter[] PROGMEM        = "FIGHTER";
char_P LOCA_Unit_Bomber[] PROGMEM         = "BOMBER";
char_P LOCA_Unit_Cruiser[] PROGMEM        = "CRUISER";
char_P LOCA_Unit_Battleship[] PROGMEM     = "BATTLESHIP";
char_P LOCA_Unit_Transportships[] PROGMEM = "TRANSPORT";

char_P* const LOCA_Unit_Names[] PROGMEM = {
  LOCA_Unit_Soldier, LOCA_Unit_Mech, LOCA_Unit_SpecOps, LOCA_Unit_scout,
  LOCA_Unit_Assist, LOCA_Unit_Tank, LOCA_Unit_BigTank, LOCA_Unit_Artillery,
  LOCA_Unit_Rocket, LOCA_Unit_Missiles, LOCA_Unit_Heli, LOCA_Unit_Fighter,
  LOCA_Unit_Bomber, LOCA_Unit_Cruiser, LOCA_Unit_Battleship, LOCA_Unit_Transportships
};

#endif
