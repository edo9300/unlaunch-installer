#include "bgMenu.h"
#include "main.h"
#include "menu.h"

#include <algorithm>
#include <nds.h>
#include <dirent.h>
#include <string>
#include <vector>

static const auto& getBackgroundList()
{
	static auto bgs = []{
		std::vector<std::pair<std::string,std::string>> bgs;
		
		for(const auto* bgstr : {"nitro:/backgrounds/", "sd:/backgrounds/"}) {
			auto* pdir = opendir(bgstr);
			if(!pdir) continue;
			dirent* pent;
			while((pent = readdir(pdir))) {
				if(pent->d_type == DT_DIR)
					continue;
				std::string name{pent->d_name};
				if(!name.ends_with(".gif") && !name.ends_with(".GIF"))
					continue;
				bgs.emplace_back(name.substr(0, name.size() - 4), bgstr + name);
			}
			closedir(pdir);
		}
		std::sort(bgs.begin(), bgs.end(), [](const auto& lhs, const auto& rhs){
			const auto& [lhs_name, lhs_fullpath] = lhs;
			const auto& [rhs_name, rhs_fullpath] = rhs;
			if(lhs_fullpath[0] != rhs_fullpath[0])
				return lhs_fullpath[0] < rhs_fullpath[0];
			return lhs_name < rhs_name;
		});
		return bgs;
	}();
	return bgs;
}

const char* backgroundMenu()
{
	//top screen
	clearScreen(&topScreen);

	//menu
	Menu* m = newMenu();
	setMenuHeader(m, "BACKGROUNDS");
	
	const auto& bgs = getBackgroundList();
	
	for(const auto& [bgName, bgPath] : bgs)
	{
		addMenuItem(m, bgName.data(), nullptr, true, false);	
	}
	addMenuItem(m, "Default", nullptr, true, false);
	addMenuItem(m, "Cancel", nullptr, true, false);

	m->cursor = 0;

	//bottom screen
	printMenu(m);

	while (!programEnd)
	{
		swiWaitForVBlank();
		scanKeys();

		if (moveCursor(m))
			printMenu(m);

		if (auto keys = keysDown(); keys & KEY_A)
			break;
		else if(keys & KEY_B)
		{
			m->cursor = bgs.size() + 1;
			break;
		}
	}

	const char* result = nullptr;
	if(static_cast<size_t>(m->cursor) < bgs.size())
		result = bgs[m->cursor].second.data();
	else if(static_cast<size_t>(m->cursor) == bgs.size())
		result = "default";
	freeMenu(m);

	return result;
}