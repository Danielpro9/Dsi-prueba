// Mouse_dsi.cpp — DSi implementation of lwjgl::Mouse.
//
// Same reasoning as Keyboard_dsi.cpp: no pointer input (touch screen,
// deferred) exists yet to call detail::pushMotion()/pushButton()/pushWheel(),
// so every query reports "no button, no motion, top-left" rather than
// leaving GUI call sites (GuiChat, GuiContainerCreative, MouseHelper, ...)
// as unresolved link errors.
#ifdef DSI_PLATFORM

#include "lwjgl/Mouse.h"

namespace lwjgl
{
namespace Mouse
{

void setCursorPosition(int_t, int_t) {}

bool next() { return false; }

int_t getEventButton()      { return -1; }
bool  getEventButtonState() { return false; }
int_t getEventDX()          { return 0; }
int_t getEventDY()          { return 0; }
int_t getEventX()           { return 0; }
int_t getEventY()           { return 0; }
int_t getEventDWheel()      { return 0; }

int_t getX() { return 0; }
int_t getY() { return 0; }

int_t getDX()     { return 0; }
int_t getDY()     { return 0; }
int_t getDWheel() { return 0; }

void clearDeltas() {}

bool isButtonDown(int_t) { return false; }

static bool s_grabbed = false;
bool isGrabbed()              { return s_grabbed; }
void setGrabbed(bool grabbed) { s_grabbed = grabbed; }

} // namespace Mouse
} // namespace lwjgl

#endif // DSI_PLATFORM
