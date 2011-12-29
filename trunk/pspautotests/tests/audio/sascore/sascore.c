#include <common.h>

#include <pspkernel.h>
#include <stdio.h>
#include "sascore.h"

//PSP_MODULE_INFO("sascore test", 0, 1, 1);
//PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

typedef struct {
	unsigned char* pointer;
	int length;
} ByteArray;

ByteArray loadData()
{
	int n;
	int length;
	FILE *file;
	ByteArray data = {0};
	if ((file = fopen("test.vag", "rb")) != NULL) {
		fseek(file, 0, SEEK_END);
		data.length = ftell(file);
		data.pointer = (char *)malloc(data.length);
		memset(data.pointer, 0, data.length);
		fseek(file, 0, SEEK_SET);
		fread(data.pointer, data.length, 1, file);
		fclose(file);
	}
	if (data.length == 0) {
		printf("DATA:Can't read file\n");
	} else {
		printf("DATA:");
		for (n = 0; n < 0x20; n++) printf("%02X", data.pointer[n]);
		printf("\n");
	}
	return data;
}

// http://www.psp-programming.com/forums/index.php?action=printpage;topic=4404.0
int main(int argc, char *argv[]) {
	SasCore sasCore;
	int sasVoice = 0;
	int result;
	int n, m;
	short samples[256 * 2] = {0};
	ByteArray data;
	
	//pspSdkLoadStartModule("flash0:/kd/sc_sascore.prx", PSP_MEMORY_PARTITION_KERNEL);
	if (pspSdkLoadStartModule("flash0:/kd/sc_sascore.prx", PSP_MEMORY_PARTITION_USER) == ERROR_KERNEL_LIBRARY_NOT_FOUND) {
		printf("Can't load library\n");
		return 0;
	}
	
	data = loadData();
	
	result = __sceSasInit(&sasCore, PSP_SAS_GRAIN_SAMPLES, PSP_SAS_VOICES_MAX, PSP_SAS_OUTPUTMODE_STEREO, 44100);
	printf("__sceSasInit: 0x%08X\n", result);
	__sceSasSetOutputmode(&sasCore, PSP_SAS_OUTPUTMODE_STEREO);
	__sceSasSetKeyOn(&sasCore, sasVoice);
	__sceSasSetVoice(&sasCore, sasVoice, data.pointer, data.length, 1);
	__sceSasSetPitch(&sasCore, sasVoice, 4096);
	__sceSasSetVolume(&sasCore, sasVoice, 0x1000, 0x1000);
	printf("Data(%d):\n", data.length);
	//for (m = 0; m < 10000; m++) printf("%d,", data.pointer[m]);
	
	for (m = 0; m < 6; m++) {
		result = __sceSasCore(&sasCore, samples);
		printf("__sceSasCore: 0x%08X\n", result);
		for (n = 0; n < 512; n++) printf("%d,", samples[n]);
		printf("\n");
	}
	printf("End\n");
	
	return 0;
}