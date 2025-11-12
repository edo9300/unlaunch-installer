#include "bgMenu.h"
#include "main.h"
#include "menu.h"
#include "gifConverter.h"
#include "message.h"

#include <algorithm>
#include <array>
#include <format>
#include <nds.h>
#include <dirent.h>
#include <memory>
#include <string>
#include <vector>

static std::array<uint8_t, MAX_GIF_SIZE> currentlyLoadedGif;

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

std::optional<std::span<uint8_t>> backgroundMenu()
{
	//top screen
	clearScreen(&topScreen);

	//menu
	auto mSptr = std::shared_ptr<Menu>(newMenu(), freeMenu);
	auto* m = mSptr.get();
	setMenuHeader(m, "BACKGROUNDS");
	
	const auto& bgs = getBackgroundList();
	
	for(const auto& [bgName, bgPath] : bgs)
	{
		addMenuItem(m, bgName.data(), nullptr, true, false);	
	}
	addMenuItem(m, "Default", nullptr, true, false);
	// addMenuItem(m, "Cancel", nullptr, true, false);

	m->cursor = 0;

	//bottom screen
	printMenu(m);

	while (!programEnd) {
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
				return {};
			}
		}

		if(programEnd)
			return std::nullopt;

		auto selection = static_cast<size_t>(m->cursor);

		if(selection == bgs.size())
			return {};
		else if (selection > bgs.size())
		{
			return std::nullopt;
		}
		try {
			const auto res = parseGif(bgs[selection].second.data(), currentlyLoadedGif, bgGetGfxPtr(bgGifTop));
			swiWaitForVBlank();
			bgHide(topScreen.bgId);
			bgShow(bgGifTop);
			auto confirmed = (choiceBox("Confirm this background?") == YES);
			swiWaitForVBlank();
			bgShow(topScreen.bgId);
			bgHide(bgGifTop);
			if(confirmed)
				return res;
		} catch(const std::exception& e) {
			messageBox(std::format("\x1B[31mError:\x1B[33m The image could not\n"
								   "be loaded: {}", e.what()).data());
		}

		printMenu(m);
	}

	return {};
}
