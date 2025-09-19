#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>

PSP_MODULE_INFO("RunTime2dPSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define printf pspDebugScreenPrintf

int main(int argc, char* argv[])
{
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    printf("RunTime2dPSP: Hello, PSP!\n");
    printf("Press X to exit.\n");

    SceCtrlData pad;

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_CROSS) break;
        sceDisplayWaitVblankStart();
    }

    sceKernelExitGame();
    return 0;
}

#ifdef PSP_BUILD_PRX
// Provide PRX entry/exit points for PSPLink/loading
int module_start(SceSize args, void* argp) { return 0; }
int module_stop(SceSize args, void* argp) { return 0; }
#endif
