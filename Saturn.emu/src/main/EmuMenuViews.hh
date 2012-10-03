#pragma once
#include "OptionView.hh"

class CDBIOSFilePicker
{
public:
	static void onSelectFile(const char* name, const InputEvent &e)
	{
		snprintf(biosPath, sizeof(FsSys::cPath), "%s/%s", FsSys::workDir(), name);
		logMsg("set bios %s", biosPath);
		View::removeModalView();
	}

	static void onClose(const InputEvent &e)
	{
		View::removeModalView();
	}

	static void init(bool highlightFirst)
	{
		fPicker.init(highlightFirst, ssBiosFsFilter);
		fPicker.onSelectFileDelegate().bind<&CDBIOSFilePicker::onSelectFile>();
		fPicker.onCloseDelegate().bind<&CDBIOSFilePicker::onClose>();
	}
};

class SystemOptionView : public OptionView
{
private:
	TextMenuItem biosPath;

	static void biosPathHandler(TextMenuItem &item, const InputEvent &e)
	{
		CDBIOSFilePicker::init(!e.isPointer());
		fPicker.placeRect(Gfx::viewportRect());
		modalView = &fPicker;
		Base::displayNeedsUpdate();
	}

	MultiChoiceSelectMenuItem sh2Core;

	void sh2CoreInit()
	{
		static const char *str[6];

		int setting = 0, cores = 0;
		iterateTimes(sizeofArray(SH2CoreList)-1, i)
		{
			if(i == sizeofArray(str))
				break;
			str[i] = SH2CoreList[i]->Name;
			if(SH2CoreList[i]->id == yinit.sh2coretype)
				setting = i;
			cores++;
		}

		sh2Core.init("SH2", str, setting, cores);
		sh2Core.valueDelegate().bind<&sh2CoreSet>();
	}

	static void sh2CoreSet(MultiChoiceMenuItem &, int val)
	{
		assert(val < (int)sizeofArray(SH2CoreList)-1);
		yinit.sh2coretype = SH2CoreList[val]->id;
	}

public:
	constexpr SystemOptionView() { }

	void loadSystemItems(MenuItem *item[], uint &items)
	{
		OptionView::loadSystemItems(item, items);
		if(sizeofArray(SH2CoreList) > 2)
		{
			sh2CoreInit(); item[items++] = &sh2Core;
		}
		biosPath.init("Select BIOS"); item[items++] = &biosPath;
		biosPath.selectDelegate().bind<&biosPathHandler>();
	}
};

#include "MenuView.hh"

class SystemMenuView : public MenuView
{
public:
	constexpr SystemMenuView() { }
};