/*  This file is part of NES.emu.

	NES.emu is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	NES.emu is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with NES.emu.  If not, see <http://www.gnu.org/licenses/> */

module;
#include <fceu/driver.h>
#include <fceu/cheat.h>

module system;

extern "C++"
{
void EncodeGG(char* str, int a, int v, int c);
void RebuildSubCheats();
}

namespace EmuEx
{

unsigned parseHex(const char* str) { return strtoul(str, nullptr, 16); }

std::string toGGString(const CheatCode& c)
{
	std::string code;
	code.resize(9);
	EncodeGG(code.data(), c.addr, c.val, c.compare);
	code.resize(8);
	return code;
}

void saveCheats()
{
	savecheats = 1;
	FCEU_FlushGameCheats(nullptr, 0, false);
}

void syncCheats()
{
	saveCheats();
	RebuildSubCheats();
}

constexpr bool isValidGGCodeLen(const char* str)
{
	return std::string_view{str}.size() == 6 || std::string_view{str}.size() == 8;
}

Cheat* NesSystem::newCheat(EmuApp& app, const char* name, CheatCodeDesc desc)
{
	auto cPtr = &static_cast<Cheat&>(cheats.emplace_back(name));
	if(!addCheatCode(app, cPtr, desc))
	{
		cheats.pop_back();
		return {};
	}
	log.info("added new cheat, {} total", cheats.size());
	return cPtr;
}

bool NesSystem::setCheatName(Cheat& c, const char* name)
{
	c.name = name;
	saveCheats();
	return true;
}

std::string_view NesSystem::cheatName(const Cheat& c) const { return c.name; }

void NesSystem::setCheatEnabled(Cheat& c, bool on)
{
	c.status = on;
	syncCheats();
}

bool NesSystem::isCheatEnabled(const Cheat& c) const { return c.status; }

bool NesSystem::addCheatCode(EmuApp& app, Cheat*& cheatPtr, CheatCodeDesc desc)
{
	if(desc.flags)
	{
		if(!isValidGGCodeLen(desc.str))
		{
			app.postMessage(true, "Invalid, must be 6 or 8 digits");
			return false;
		}
		uint16 a; uint8 v; int c;
		if(!FCEUI_DecodeGG(desc.str, &a, &v, &c))
		{
			app.postMessage(true, "Error decoding code");
			return false;
		}
		cheatPtr->codes.emplace_back(a, v, c, 1);
	}
	else
	{
		auto a = parseHex(desc.str);
		if(a > 0xFFFF)
		{
			app.postMessage(true, "Invalid address");
			return false;
		}
		cheatPtr->codes.emplace_back(a, 0, -1, 0);
	}
	syncCheats();
	return true;
}

bool NesSystem::modifyCheatCode(EmuApp& app, Cheat&, CheatCode& c, CheatCodeDesc desc)
{
	assume(desc.flags);
	if(!isValidGGCodeLen(desc.str))
	{
		app.postMessage(true, "Invalid, must be 6 or 8 digits");
		return false;
	}
	if(!FCEUI_DecodeGG(desc.str, &c.addr, &c.val, &c.compare))
	{
		app.postMessage(true, "Error decoding code");
		return false;
	}
	syncCheats();
	return true;
}

Cheat* NesSystem::removeCheatCode(Cheat& c, CheatCode& code)
{
	c.codes.erase(toIterator(c.codes, static_cast<CHEATCODE&>(code)));
	bool removedAllCodes = c.codes.empty();
	if(removedAllCodes)
		cheats.erase(toIterator(cheats, static_cast<CHEATF&>(c)));
	syncCheats();
	return removedAllCodes ? nullptr : &c;
}

bool NesSystem::removeCheat(Cheat& c)
{
	cheats.erase(toIterator(cheats, static_cast<CHEATF&>(c)));
	syncCheats();
	return true;
}

void NesSystem::forEachCheat(DelegateFunc<bool(Cheat&, std::string_view)> del)
{
	for(auto& c: cheats)
	{
		if(!del(static_cast<Cheat&>(c), std::string_view{c.name}))
			break;
	}
}

void NesSystem::forEachCheatCode(Cheat& cheat, DelegateFunc<bool(CheatCode&, std::string_view)> del)
{
	for(auto& c_: cheat.codes)
	{
		auto& c = static_cast<CheatCode&>(c_);
		std::string code;
		if(c.type)
		{
			code = toGGString(c);
		}
		else
		{
			code = std::format("{:x}:{:x}", c.addr, c.val);
			if(c.compare != -1)
				code += std::format(":{:x}", c.compare);
		}
		del(c, std::string_view{code});
	}
}

//region 爱吾修改
static std::vector<std::string> split(std::string s,char ch)
{
    int start=0;
    int len=0;
    std::vector<std::string> ret;
    for(int i=0;i<s.length();i++){
        if(s[i]==ch){
            ret.push_back(s.substr(start,len));
            start=i+1;
            len=0;
        }
        else{
            len++;
        }
    }
    if(start<s.length())
        ret.push_back(s.substr(start,len));
    return ret;
}
void NesSystem::setCheatListAiWu(const std::list<std::string>& cheatList)
{
    if(!hasContent())
        return;
    //先清空金手指
	cheats.clear();
    //再添加新的金手指
	for (const std::string& cheat : cheatList)
	{
        //先判断是不是RAM
        const char *str = cheat.c_str();
		bool isRam = std::string_view{str}.find('-') != std::string_view::npos;
        if(isRam){//RAM
            std::vector<std::string> strs = split(cheat,'-');
        	if (strs.empty())
        	{
        		continue;
        	}
            //地址
            unsigned address = parseHex(strs[0].c_str());
            if(address > 0xFFFF)
            {
                continue;
            }
            //比较值（可选）
            int compare = -1;
            if(strs.size() == 3)
            {
                compare = parseHex(strs[1].c_str());
                if(compare > 0xFF)
                {
                    continue;
                }
            }
            //值
            int lastIndex = strs.size()-1;
            unsigned value = parseHex(strs[lastIndex].c_str());
            if(value > 0xFF)
            {
                continue;
            }
        	auto cheatPtr = &static_cast<Cheat&>(cheats.emplace_back("RAM Cheat"));
        	cheatPtr->codes.emplace_back(address, value, compare, 0);
			cheatPtr->status = true;
        } else {//GG
            if(!isValidGGCodeLen(str))
            {
                continue;
            }
            uint16 address; uint8 value; int compare;
            if(!FCEUI_DecodeGG(str, &address, &value, &compare))
            {
                continue;
            }
        	auto cheatPtr = &static_cast<Cheat&>(cheats.emplace_back("GG Cheat"));
        	cheatPtr->codes.emplace_back(address, value, compare, 1);
			cheatPtr->status = true;
        }
    }
	log.info("new cheat count:{}", cheats.size());
    //刷新金手指
	RebuildSubCheats();
}
//endregion
}
