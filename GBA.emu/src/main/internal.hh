#pragma once

#include <emuframework/Option.hh>
#include <list>

static const uint RTC_EMU_AUTO = 0, RTC_EMU_OFF = 1, RTC_EMU_ON = 2;

extern Byte1Option optionRtcEmulation;
extern bool detectedRtcGame;

void setRTC(uint mode);
void readCheatFile();
void writeCheatFile();

void setCheatListForAiWu(std::list<std::string> cheats);
