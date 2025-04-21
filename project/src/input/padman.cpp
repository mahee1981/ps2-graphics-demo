#include "input/padman.hpp"

namespace Input {

void PadManager::LoadModules()
{
    int ret;

    ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
    if (ret < 0) {
        printf("sifLoadModule sio failed: %d\n", ret);
        SleepThread();
    }

    ret = SifLoadModule("rom0:PADMAN", 0, NULL);
    if (ret < 0) {
        printf("sifLoadModule pad failed: %d\n", ret);
        SleepThread();
    }
}

void PadManager::WaitPadReady(int port, int slot)
{
    int state;
    int lastState;
    char stateString[16];

    state = padGetState(port, slot);
    lastState = -1;
    while ((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1)) {
        if (state != lastState) {
            padStateInt2String(state, stateString);
            printf("Please wait, pad(%d,%d) is in state %s\n",
                port, slot, stateString);
        }
        lastState = state;
        state = padGetState(port, slot);
    }
    // Were the pad ever 'out of sync'?
    if (lastState != -1) {
        printf("Pad OK!\n");
    }
}

int PadManager::InitializePad(int port, int slot)
{
    int ret;
    int modes;
    int i;

    WaitPadReady(port, slot);

    // How many different modes can this device operate in?
    // i.e. get # entrys in the modetable
    modes = padInfoMode(port, slot, PAD_MODETABLE, -1);
    printf("The device has %d modes\n", modes);

    if (modes > 0) {
        printf("( ");
        for (i = 0; i < modes; i++) {
            printf("%d ", padInfoMode(port, slot, PAD_MODETABLE, i));
        }
        printf(")");
    }

    printf("It is currently using mode %d\n",
        padInfoMode(port, slot, PAD_MODECURID, 0));

    // If modes == 0, this is not a Dual shock controller
    // (it has no actuator engines)
    if (modes == 0) {
        printf("This is a digital controller?\n");
        return 1;
    }

    // Verify that the controller has a DUAL SHOCK mode
    i = 0;
    do {
        if (padInfoMode(port, slot, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK)
            break;
        i++;
    } while (i < modes);
    if (i >= modes) {
        printf("This is no Dual Shock controller\n");
        return 1;
    }

    // If ExId != 0x0 => This controller has actuator engines
    // This check should always pass if the Dual Shock test above passed
    ret = padInfoMode(port, slot, PAD_MODECUREXID, 0);
    if (ret == 0) {
        printf("This is no Dual Shock controller??\n");
        return 1;
    }

    printf("Enabling dual shock functions\n");

    // When using MMODE_LOCK, user cant change mode with Select button
    padSetMainMode(port, slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);

    WaitPadReady(port, slot);
    printf("infoPressMode: %d\n", padInfoPressMode(port, slot));

    WaitPadReady(port, slot);
    printf("enterPressMode: %d\n", padEnterPressMode(port, slot));

    WaitPadReady(port, slot);
    actuators = padInfoAct(port, slot, -1, 0);
    printf("# of actuators: %d\n", actuators);

    if (actuators != 0) {
        actAlign[0] = 0; // Enable small engine
        actAlign[1] = 1; // Enable big engine
        actAlign[2] = 0xff;
        actAlign[3] = 0xff;
        actAlign[4] = 0xff;
        actAlign[5] = 0xff;

        WaitPadReady(port, slot);
        printf("padSetActAlign: %d\n",
            padSetActAlign(port, slot, actAlign));
    } else {
        printf("Did not find any actuators.\n");
    }

    WaitPadReady(port, slot);
    return 1;
}

PadManager::PadManager()
{
    SifInitRpc(0);

    LoadModules();

    padInit(0);
    int ret;
    port = 0;
    slot = 0;

    old_pad = 0;

    if ((ret = padPortOpen(port, slot, padBuffer)) == 0) {
        printf("padOpenPort failed: %d\n", ret);
        SleepThread();
    }

    if (!InitializePad(port, slot)) {
        printf("pad initalization failed!\n");
        SleepThread();
    }
}

void PadManager::UpdatePad()
{
    int ret;
    ret = padGetState(port, slot);
    while ((ret != PAD_STATE_STABLE) && (ret != PAD_STATE_FINDCTP1)) {
        if (ret == PAD_STATE_DISCONN) {
            printf("Pad(%d, %d) is disconnected\n", port, slot);
        }
        ret = padGetState(port, slot);
    }

    ret = padRead(port, slot, &buttons); // port, slot, buttons

    if (ret != 0) {
        paddata = 0xffff ^ buttons.btns;

        new_pad = paddata & ~old_pad;
        old_pad = paddata;
        this->reset();

        // Digital buttons
        this->rightJoyPad.h = this->buttons.rjoy_h;
        this->rightJoyPad.v = this->buttons.rjoy_v;
        this->leftJoyPad.h = this->buttons.ljoy_h;
        this->leftJoyPad.v = this->buttons.ljoy_v;

        this->rightJoyPad.isCentered = this->buttons.rjoy_h == 127 && this->buttons.rjoy_v == 127;
        this->rightJoyPad.isMoved = !this->rightJoyPad.isCentered;

        this->leftJoyPad.isCentered = this->buttons.ljoy_h == 127 && this->buttons.ljoy_v == 127;
        this->leftJoyPad.isMoved = !this->leftJoyPad.isCentered;

        this->handleClickedButtons();
        this->handlePressedButtons();
    }
}

/** Update clicked buttons state */
void PadManager::handleClickedButtons() {
  if (this->new_pad & PAD_CROSS) this->clicked.Cross = 1;
  if (this->new_pad & PAD_SQUARE) this->clicked.Square = 1;
  if (this->new_pad & PAD_TRIANGLE) this->clicked.Triangle = 1;
  if (this->new_pad & PAD_CIRCLE) this->clicked.Circle = 1;
  if (this->new_pad & PAD_UP) this->clicked.DpadUp = 1;
  if (this->new_pad & PAD_DOWN) this->clicked.DpadDown = 1;
  if (this->new_pad & PAD_LEFT) this->clicked.DpadLeft = 1;
  if (this->new_pad & PAD_RIGHT) this->clicked.DpadRight = 1;
  if (this->new_pad & PAD_L1) this->clicked.L1 = 1;
  if (this->new_pad & PAD_L2) this->clicked.L2 = 1;
  if (this->new_pad & PAD_L3) this->clicked.L3 = 1;
  if (this->new_pad & PAD_R1) this->clicked.R1 = 1;
  if (this->new_pad & PAD_R2) this->clicked.R2 = 1;
  if (this->new_pad & PAD_R3) this->clicked.R3 = 1;
  if (this->new_pad & PAD_START) this->clicked.Start = 1;
  if (this->new_pad & PAD_SELECT) this->clicked.Select = 1;
}

/** Update pressed buttons state */
void PadManager::handlePressedButtons() {
  if (this->buttons.cross_p) this->pressed.Cross = 1;
  if (this->buttons.square_p) this->pressed.Square = 1;
  if (this->buttons.triangle_p) this->pressed.Triangle = 1;
  if (this->buttons.circle_p) this->pressed.Circle = 1;
  if (this->buttons.up_p) this->pressed.DpadUp = 1;
  if (this->buttons.down_p) this->pressed.DpadDown = 1;
  if (this->buttons.left_p) this->pressed.DpadLeft = 1;
  if (this->buttons.right_p) this->pressed.DpadRight = 1;
  if (this->buttons.l1_p) this->pressed.L1 = 1;
  if (this->buttons.l2_p) this->pressed.L2 = 1;
  if (this->buttons.r1_p) this->pressed.R1 = 1;
  if (this->buttons.r2_p) this->pressed.R2 = 1;
}

/** Resets state of joys/buttons */
void PadManager::reset() {
  this->clicked.Cross = 0;
  this->clicked.Square = 0;
  this->clicked.Triangle = 0;
  this->clicked.Circle = 0;
  this->clicked.DpadUp = 0;
  this->clicked.DpadDown = 0;
  this->clicked.DpadLeft = 0;
  this->clicked.DpadRight = 0;
  this->clicked.L1 = 0;
  this->clicked.L2 = 0;
  this->clicked.L3 = 0;
  this->clicked.R1 = 0;
  this->clicked.R2 = 0;
  this->clicked.R3 = 0;
  this->clicked.Start = 0;
  this->clicked.Select = 0;

  this->pressed.Cross = 0;
  this->pressed.Square = 0;
  this->pressed.Triangle = 0;
  this->pressed.Circle = 0;
  this->pressed.DpadUp = 0;
  this->pressed.DpadDown = 0;
  this->pressed.DpadLeft = 0;
  this->pressed.DpadRight = 0;
  this->pressed.L1 = 0;
  this->pressed.L2 = 0;
  this->pressed.R1 = 0;
  this->pressed.R2 = 0;
}

}
