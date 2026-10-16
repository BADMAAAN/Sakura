#pragma once
#include "widgets.h"
#include "typography.h"
#include <stdio.h>

namespace delta { namespace ui {
// A category keeps the existing options and encoder handlers; this only paints their real state.
inline void wifiMain(uint16_t*p,const Appearance&a,const MenuRow*rows,int count,int selected,const Status*status=nullptr,const char*connection=nullptr,int focusY=-1) {
    modeList(p,a,"Wi-Fi","",rows,count,selected,status,focusY);
}
}} // namespace delta::ui
