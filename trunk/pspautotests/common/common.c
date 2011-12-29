#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pspdebug.h>
#include <pspthreadman.h>
#include <psploadexec.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>
//#include <pspkdebug.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/lock.h>
#include <sys/fcntl.h>
//#include "local.h"

/*
enum PspThreadAttributes
{
	PSP_THREAD_ATTR_VFPU = 0x00004000,
	PSP_THREAD_ATTR_USER = 0x80000000,
	PSP_THREAD_ATTR_USBWLAN = 0xa0000000,
	PSP_THREAD_ATTR_VSH = 0xc0000000,
	PSP_THREAD_ATTR_SCRATCH_SRAM = 0x00008000,
	PSP_THREAD_ATTR_NO_FILLSTACK = 0x00100000,
	PSP_THREAD_ATTR_CLEAR_STACK = 0x00200000,
};
*/
/*
enum PspModuleInfoAttr
{
	PSP_MODULE_USER			= 0,
	PSP_MODULE_NO_STOP		= 0x0001,
	PSP_MODULE_SINGLE_LOAD	= 0x0002,
	PSP_MODULE_SINGLE_START	= 0x0004,
	PSP_MODULE_KERNEL		= 0x1000,
};
*/

PSP_MODULE_INFO("TESTMODULE", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
//PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

#define EMULATOR_DEVCTL__GET_HAS_DISPLAY 0x00000001
#define EMULATOR_DEVCTL__SEND_OUTPUT     0x00000002
#define EMULATOR_DEVCTL__IS_EMULATOR     0x00000003

unsigned int RUNNING_ON_EMULATOR = 0;
unsigned int HAS_DISPLAY = 1;

extern int test_main(int argc, char *argv[]);

FILE stdout_back = {NULL};
//int KprintfFd = 0;

static int writeStdoutHook(struct _reent *ptr, void *cookie, const char *buf, int buf_len) {
	char temp[1024 + 1];

	//if (KprintfFd > 0) sceIoWrite(KprintfFd, buf, buf_len);
	if (RUNNING_ON_EMULATOR) {
		sceIoDevctl("emulator:", EMULATOR_DEVCTL__SEND_OUTPUT, (void *)buf, buf_len, NULL, 0);
	}

	if (buf_len < sizeof(temp)) {
		if (HAS_DISPLAY) {
			memcpy(temp, buf, buf_len);
			temp[buf_len] = 0;

			//Kprintf("%s", temp);
			pspDebugScreenPrintf("%s", temp);
		}
	}
	
	if (stdout_back._write != NULL) {
		return stdout_back._write(ptr, cookie, buf, buf_len);
	} else {
		return buf_len;
	}
}

void test_begin() {
	if (HAS_DISPLAY) {
		pspDebugScreenInit();
	}

	if (RUNNING_ON_EMULATOR) {
		fclose(stdout);
		stdout = fmemopen(alloca(4), 4, "wb");
		stdout_back._write = NULL;
		
		//stderr = stdout;
		setbuf(stdout, NULL);
	} else {
		freopen("ms0:/__testoutput.txt", "wb", stdout);
		freopen("ms0:/__testerror.txt", "wb", stderr);
		stdout_back._write = stdout->_write;
	}
	stdout->_write = writeStdoutHook;
	
	setbuf(stderr, NULL);
}

void test_end() {
	fflush(stdout);
	fflush(stderr);

	if (!RUNNING_ON_EMULATOR) {
		SceCtrlData key;
		while (1) {
			sceCtrlReadBufferPositive(&key, 1);
			if (key.Buttons & PSP_CTRL_CROSS) break;
		}
	}
	
	//fclose(stdout);
	sceKernelExitGame();
	
	exit(0);
}

int test_psp_exit_callback(int arg1, int arg2, void *common) {
	exit(0);
	return 0;
}

int test_psp_callback_thread(SceSize args, void *argp) {
	int cbid;
	cbid = sceKernelCreateCallback("Exit Callback", test_psp_exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();
	return 0;
}

int test_psp_setup_callbacks(void) {
	int thid = 0;
	thid = sceKernelCreateThread("update_thread",  test_psp_callback_thread, 0x11, 0xFA0, 0, 0);
	if (thid >= 0) sceKernelStartThread(thid, 0, 0);
	return thid;
}

//#define START_WITH "ms0:/PSP/GAME/virtual"

/*
void emitInt(int v) {
	asm("syscall 0x1010");
}

void emitFloat(float v) {
	asm("syscall 0x1011");
}

void emitString(char *v) {
	asm("syscall 0x1012");
}

void emitComment(char *v) {
	asm("syscall 0x1012");
}

void emitMemoryBlock(void *address, unsigned int size) {
	asm("syscall 0x1013");
}

void emitHex(void *address, unsigned int size) {
	asm("syscall 0x1014");
}
*/

int main(int argc, char *argv[]) {
	int retval = 0;

	//KprintfFd = sceIoOpen("emulator:/Kprintf", O_WRONLY, 0777);
	//RUNNING_ON_EMULATOR = (KprintfFd > 0);
	
	if (sceIoDevctl("kemulator:", EMULATOR_DEVCTL__IS_EMULATOR, NULL, 0, NULL, 0) == 0) {
		RUNNING_ON_EMULATOR = 1;
	}
	
	// TEMP HACK
	//RUNNING_ON_EMULATOR = 0;

	if (RUNNING_ON_EMULATOR) {
		sceIoDevctl("kemulator:", EMULATOR_DEVCTL__GET_HAS_DISPLAY, NULL, 0, &HAS_DISPLAY, sizeof(HAS_DISPLAY));
	}

	//if (strncmp(argv[0], START_WITH, strlen(START_WITH)) == 0) RUNNING_ON_EMULATOR = 1;

	if (!RUNNING_ON_EMULATOR) {
		test_psp_setup_callbacks();
	}
	atexit(sceKernelExitGame);

	test_begin();
	{
		pspDebugScreenPrintf("RUNNING_ON_EMULATOR: %s - %s\n", RUNNING_ON_EMULATOR ? "yes" : "no", argv[0]);
		retval = test_main(argc, argv);
	}
	test_end();
	
	return retval;
}
