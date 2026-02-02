#ifndef PADMAN_HPP
#define PADMAN_HPP

#include <debug.h>
#include <iopcontrol.h>
#include <kernel.h>
#include <libpad.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <tamtypes.h>

namespace Input
{

struct PadButtons
{
    u8 Cross, Square, Triangle, Circle, DpadUp, DpadDown, DpadLeft, DpadRight, L1, L2, L3, R1, R2, R3, Start, Select;
};

struct PadJoy
{
    u8 h, v, isCentered, isMoved;
};

class PadManager
{
  private:
    char padBuffer[256] __attribute__((aligned(64)));
    char actAlign[6];
    int actuators;

    int port, slot, ret, state;
    struct padButtonStatus buttons;
    u32 paddata;
    u32 old_pad = 0;
    u32 new_pad;
    PadButtons pressed, clicked;
    PadJoy leftJoyPad, rightJoyPad;

    void LoadModules();
    void reset();
    void handleClickedButtons();
    void handlePressedButtons();
    int WaitPadReady(int port, int slot);

  public:
    PadManager();
    PadManager(bool resetIop);
    int InitializePad(int port, int slot);
    void UpdatePad();
    inline const PadButtons &getClicked() const
    {
        return clicked;
    }
    inline const PadButtons &getPressed() const
    {
        return pressed;
    }
    inline const PadJoy &getLeftJoyPad() const
    {
        return leftJoyPad;
    }
    inline const PadJoy &getRightJoyPad() const
    {
        return rightJoyPad;
    }
};

} // namespace Input

#endif
