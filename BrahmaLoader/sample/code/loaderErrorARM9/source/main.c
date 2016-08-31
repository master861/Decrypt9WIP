#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "draw.h"
#include "hid.h"
#include "i2c.h"


void Reboot()
{
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2);
    while(true);
}

void ClearTop() {
	ClearScreen(TOP_SCREEN0, BG_COLOR);
	ClearScreen(TOP_SCREEN1, BG_COLOR);
	current_y = 0;
}

int main() {
	ClearTop();
    Debug("");
    Debug("* BRAHMA2LOADER *");
    Debug("");
	Debug("Seems like something went wrong here!");
    Debug("");
    Debug("Next time, you should specify a payload.");
    Debug("To do so you can... (before compiling):");
    Debug("- put a payload.bin file into data/.");
    Debug("- edit the AppInfo file in resources/.");
    Debug("");
    Debug("You may also use XML start parameters in");
    Debug("*hax 2.5 to specify an ARM9 payload.");
    Debug("");
    Debug("");
	Debug("<press any key to reboot>");
	InputWait();

    // reboot using i2c
    Reboot();
    
	// return control to FIRM ARM9 code (performs firmlaunch)
	return 0;
}