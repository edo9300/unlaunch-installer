#ifndef MENU_H
#define MENU_H

#include <nds/ndstypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ITEMS_PER_PAGE 20

typedef struct {
	bool directory;
	bool enabled;
	char* label;
	char* value;
} Item;

typedef struct {
	int cursor;
	int page;
	int itemCount;
	bool nextPage;
	int changePage;
	char header[32];
	Item items[ITEMS_PER_PAGE];
} Menu;

Menu* newMenu();
void freeMenu(Menu* m);

void addMenuItem(Menu* m, char const* label, char const* value, bool enabled, bool directory);
void sortMenuItems(Menu* m);
void setMenuHeader(Menu* m, const char* str);

void resetMenu(Menu* m);
void clearMenu(Menu* m);
void printMenu(Menu* m);

bool moveCursor(Menu* m);

#ifdef __cplusplus
}
#endif

#endif