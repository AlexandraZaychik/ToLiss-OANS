#pragma once

namespace oans {
struct MenuEntry {const char* id;const char* parent;const char* label;int action;};
inline constexpr MenuEntry menu_layout[]={
#define OANS_MENU(id,parent,label,action) {#id,#parent,label,action},
#include "menu_layout.def"
#undef OANS_MENU
};
}
