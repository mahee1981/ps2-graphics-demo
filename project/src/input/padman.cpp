#include "input/padman.hpp"



namespace Input
{

    void PadManager::LoadModules()
    {
        int ret;

        printf("loading sif module\n");

        ret = SifLoadModule("rom0:XSIO2MAN", 0, NULL);
        if (ret < 0)
        {
            printf("sifLoadModule sio failed: %d\n", ret);
            SleepThread();
        }
        else
        {
            printf("loaded sif module\n");
        }

        printf("loading padman module\n");

        ret = SifLoadModule("rom0:XPADMAN", 0, NULL);
        if (ret < 0)
        {
            printf("sifLoadModule pad failed: %d\n", ret);
        }
        else
        {
            printf("loaded padman module\n");
        }

        printf("LoadedModeules\n");
    }

    int PadManager::WaitPadReady(int port, int slot)
    {
        int state;

        // busy wait for the pad to get ready
        do
        {
            state = padGetState(port, slot);
        } while ((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1) && (state != PAD_STATE_DISCONN));

        return state;
    }

    int PadManager::InitializePad(int port, int slot)
    {
        int tmp;
        int modes;
        int i;

        printf("PAD initializing pad %d,%d\n", port, slot);

        // is there any device connected to that port?
        if (WaitPadReady(port, slot) == PAD_STATE_DISCONN)
        {
            printf("PAD pad %d,%d not connected.\n", port, slot);
            return 1; // nope, don't waste your time here!
        }

        // How many different modes can this device operate in?
        // i.e. get # entrys in the modetable
        modes = padInfoMode(port, slot, PAD_MODETABLE, -1);
        printf("PAD The device has %d modes: ", modes);

        if (modes > 0)
        {
            printf("( ");

            for (i = 0; i < modes; i++)
            {
                tmp = padInfoMode(port, slot, PAD_MODETABLE, i);
                printf("%d ", tmp);
            }

            printf(")\n");
        }

        tmp = padInfoMode(port, slot, PAD_MODECURID, 0);
        printf("PAD It is currently using mode %d\n", tmp);

        // If modes == 0, this is not a Dual shock controller
        // (it has no actuator engines)
        if (modes == 0)
        {
            printf("PAD This is a digital controller?\n");
            return 1;
        }

        // Verify that the controller has a DUAL SHOCK mode
        i = 0;
        do
        {
            if (padInfoMode(port, slot, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK)
                break;
            i++;
        } while (i < modes);

        if (i >= modes)
        {
            printf("PAD This is no Dual Shock controller\n");
            return 1;
        }

        // If ExId != 0x0 => This controller has actuator engines
        // This check should always pass if the Dual Shock test above passed
        tmp = padInfoMode(port, slot, PAD_MODECUREXID, 0);
        if (tmp == 0)
        {
            printf("PAD This is no Dual Shock controller??\n");
            return 1;
        }

        printf("PAD Enabling dual shock functions\n");

        // When using MMODE_LOCK, user cant change mode with Select button
        padSetMainMode(port, slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);

        WaitPadReady(port, slot);
        tmp = padInfoPressMode(port, slot);
        printf("PAD infoPressMode: %d\n", tmp);

        WaitPadReady(port, slot);
        tmp = padEnterPressMode(port, slot);
        printf("PAD enterPressMode: %d\n", tmp);

        WaitPadReady(port, slot);
        this->actuators = padInfoAct(this->port, this->slot, -1, 0);
        printf("PAD # of actuators: %d\n", this->actuators);

        if (this->actuators != 0)
        {
            this->actAlign[0] = 0; // Enable small engine
            this->actAlign[1] = 1; // Enable big engine
            this->actAlign[2] = 0xff;
            this->actAlign[3] = 0xff;
            this->actAlign[4] = 0xff;
            this->actAlign[5] = 0xff;

            WaitPadReady(port, slot);
            tmp = padSetActAlign(this->port, this->slot, this->actAlign);
            printf("PAD padSetActAlign: %d\n", tmp);
        }
        else
        {
            printf("PAD Did not find any actuators.\n");
        }

        WaitPadReady(port, slot);

        return 1;
    }

    PadManager::PadManager()
    {

        SifInitRpc(0);

        // Reinit subsystems after reset

        while (!SifIopReset("", 0))
        {
        };
        while (!SifIopSync())
        {
        };

        SifInitRpc(0);

        LoadModules();

        padInit(0);
        int ret;
        port = 0;
        slot = 0;

        old_pad = 0;

        if ((ret = padPortOpen(port, slot, padBuffer)) == 0)
        {
            printf("padOpenPort failed: %d\n", ret);
            SleepThread();
        }

        if (!InitializePad(port, slot))
        {
            printf("pad initalization failed!\n");
            SleepThread();
        }
        state = padGetState(port, slot);
    }

    void PadManager::UpdatePad()
    {
        int oldState = state;
        state = padGetState(port, slot);

        if ((oldState == PAD_STATE_DISCONN) && ((this->state == PAD_STATE_STABLE) || (this->state == PAD_STATE_FINDCTP1)))
        {
            // Pad just connected.
            printf("PAD pad %d,%d connected\n", this->port, this->slot);
            InitializePad(port, slot);
        }
        // The pad may transit from any state to disconnected. So check only for the disconnected state.
        else if ((oldState != PAD_STATE_DISCONN) && (this->state == PAD_STATE_DISCONN))
        {
            printf("PAD pad %d,%d disconnected\n", this->port, this->slot);
        }

        if ((this->state == PAD_STATE_STABLE) || (this->state == PAD_STATE_FINDCTP1))
        {
            int ret;
            // pad is connected. Read pad button information.
            ret = padRead(this->port, this->slot, &this->buttons); // port, slot, buttons

            if (ret != 0)
            {
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
    }

    /** Update clicked buttons state */
    void PadManager::handleClickedButtons()
    {
        if (this->new_pad & PAD_CROSS)
            this->clicked.Cross = 1;
        if (this->new_pad & PAD_SQUARE)
            this->clicked.Square = 1;
        if (this->new_pad & PAD_TRIANGLE)
            this->clicked.Triangle = 1;
        if (this->new_pad & PAD_CIRCLE)
            this->clicked.Circle = 1;
        if (this->new_pad & PAD_UP)
            this->clicked.DpadUp = 1;
        if (this->new_pad & PAD_DOWN)
            this->clicked.DpadDown = 1;
        if (this->new_pad & PAD_LEFT)
            this->clicked.DpadLeft = 1;
        if (this->new_pad & PAD_RIGHT)
            this->clicked.DpadRight = 1;
        if (this->new_pad & PAD_L1)
            this->clicked.L1 = 1;
        if (this->new_pad & PAD_L2)
            this->clicked.L2 = 1;
        if (this->new_pad & PAD_L3)
            this->clicked.L3 = 1;
        if (this->new_pad & PAD_R1)
            this->clicked.R1 = 1;
        if (this->new_pad & PAD_R2)
            this->clicked.R2 = 1;
        if (this->new_pad & PAD_R3)
            this->clicked.R3 = 1;
        if (this->new_pad & PAD_START)
            this->clicked.Start = 1;
        if (this->new_pad & PAD_SELECT)
            this->clicked.Select = 1;
    }

    /** Update pressed buttons state */
    void PadManager::handlePressedButtons()
    {
        if (this->buttons.cross_p)
            this->pressed.Cross = 1;
        if (this->buttons.square_p)
            this->pressed.Square = 1;
        if (this->buttons.triangle_p)
            this->pressed.Triangle = 1;
        if (this->buttons.circle_p)
            this->pressed.Circle = 1;
        if (this->buttons.up_p)
            this->pressed.DpadUp = 1;
        if (this->buttons.down_p)
            this->pressed.DpadDown = 1;
        if (this->buttons.left_p)
            this->pressed.DpadLeft = 1;
        if (this->buttons.right_p)
            this->pressed.DpadRight = 1;
        if (this->buttons.l1_p)
            this->pressed.L1 = 1;
        if (this->buttons.l2_p)
            this->pressed.L2 = 1;
        if (this->buttons.r1_p)
            this->pressed.R1 = 1;
        if (this->buttons.r2_p)
            this->pressed.R2 = 1;
    }

    /** Resets state of joys/buttons */
    void PadManager::reset()
    {
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
