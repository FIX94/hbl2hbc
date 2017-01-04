/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <vector>
#include <string>
#include "dynamic_libs/fs_functions.h"
#include "dynamic_libs/os_functions.h"
#include "dynamic_libs/sys_functions.h"
#include "dynamic_libs/padscore_functions.h"
#include "dynamic_libs/vpad_functions.h"
#include "fs/sd_fat_devoptab.h"
#include "system/memory.h"
#include "utils/utils.h"
#include "common/common.h"

struct selTitle {
	u32 id;
	std::string name;
};

static const char *verChar = "HBL2HBC v1.1u1 by FIX94";

static unsigned int getButtonsDown();
static bool doIdSelect();

static u32 titleToBoot = 0;

#define OSScreenEnable(enable) OSScreenEnableEx(0, enable); OSScreenEnableEx(1, enable);
#define OSScreenClearBuffer(tmp) OSScreenClearBufferEx(0, tmp); OSScreenClearBufferEx(1, tmp);
#define OSScreenPutFont(x, y, buf) OSScreenPutFontEx(0, x, y, buf); OSScreenPutFontEx(1, x, y, buf);
#define OSScreenFlipBuffers() OSScreenFlipBuffersEx(0); OSScreenFlipBuffersEx(1);

extern "C" int Menu_Main(void)
{
	InitOSFunctionPointers();
	InitSysFunctionPointers();
	InitPadScoreFunctionPointers();

	unsigned long long sysmenu = _SYSGetSystemApplicationTitleId(0);
	if(OSGetTitleID() != sysmenu)
	{
		if(doIdSelect() == false)
			return EXIT_SUCCESS;
		//CMPT only seems to work properly from system menu
		SYSLaunchMenu();
		return EXIT_RELAUNCH_ON_LOAD;
	}

	//get all the CMPT functions
	unsigned int cmpt_handle;
	OSDynLoad_Acquire("nn_cmpt.rpl", &cmpt_handle);

	int (*CMPTLaunchTitle)(void* CMPTConfigure, int ConfigSize, int titlehigh, int titlelow);
	int (*CMPTAcctSetScreenType)(int screenType);
	int (*CMPTGetDataSize)(int* dataSize);
	int (*CMPTCheckScreenState)();

	OSDynLoad_FindExport(cmpt_handle, 0, "CMPTLaunchTitle", &CMPTLaunchTitle);
	OSDynLoad_FindExport(cmpt_handle, 0, "CMPTAcctSetScreenType", &CMPTAcctSetScreenType);
	OSDynLoad_FindExport(cmpt_handle, 0, "CMPTGetDataSize", &CMPTGetDataSize);
	OSDynLoad_FindExport(cmpt_handle, 0, "CMPTCheckScreenState", &CMPTCheckScreenState);

	//1 = TV Only, 2 = GamePad Only, 3 = Both
	CMPTAcctSetScreenType(3); 
	if(CMPTCheckScreenState() < 0)
	{
		CMPTAcctSetScreenType(2);
		if(CMPTCheckScreenState() < 0)
			CMPTAcctSetScreenType(1);
	}

	int datasize = 0;
	void *databuf;
	int result = CMPTGetDataSize(&datasize);
	if(result < 0)
		goto prgEnd;

	//needed for CMPT to work
	KPADInit();

	databuf = memalign(0x40, datasize);
	CMPTLaunchTitle(databuf, datasize, 0x00010001, titleToBoot);
	free(databuf);

prgEnd:	;
	//will launch vwii if launchtitle was successful
	return EXIT_SUCCESS;
}

/* Select Bootup ID */

static bool doIdSelect()
{
	bool ret = true;

	InitFSFunctionPointers();
	InitVPadFunctionPointers();

	memoryInitialize();
	mount_sd_fat("sd");

	int screen_buf0_size, screen_buf1_size;
	uint8_t *screenBuffer;

	s32 PosX = 0;
	s32 ScrollX = 0;
	s32 ListMax;
	int redraw = 1;

	std::vector<selTitle> selTitles;
	int entries = 0;
	int cPos = 0;
	int flen;
	char *cBuf;

	FILE *f = fopen("sd:/hbl2hbc.txt","rb");
	if(!f)
	{
		//current vwii hbc title id
		titleToBoot = 0x4c554c5a;
		goto func_exit;
	}
	fseek(f,0,SEEK_END);
	flen = ftell(f);
	rewind(f);
	cBuf = (char*)malloc(flen+1);
	fread(cBuf,1,flen,f);
	cBuf[flen] = '\0';
	fclose(f);

	while(cPos < flen)
	{
		if(cBuf[cPos+4] == '=')
		{
			u32 curTID;
			memcpy(&curTID, cBuf+cPos, 4);
			char *fEnd = NULL;
			char *fName = cBuf+cPos+5;
			char *fEndR = strchr(fName,'\r');
			char *fEndN = strchr(fName,'\n');
			if(fEndR)
			{
				if(fEndN && fEndN < fEndR)
					fEnd = fEndN;
				else
					fEnd = fEndR;
			}
			else if(fEndN)
			{
				if(fEndR && fEndR < fEndN)
					fEnd = fEndR;
				else
					fEnd = fEndN;
			}
			else
				fEnd = fName+strlen(fName);
			if(fEnd && fName < fEnd)
			{
				size_t fLen = (fEnd - fName);
				selTitles.push_back({curTID, std::string(fName, fLen)});
				cPos += fLen;
				entries++;
			}
			else
				cPos++;
		}
		else
			cPos++;
	}
	free(cBuf);

	if(entries == 0)
	{
		//current vwii hbc title id
		titleToBoot = 0x4c554c5a;
		goto func_exit;
	}
	else if(entries == 1)
	{
		//no point in displaying 1 option
		titleToBoot = selTitles[0].id;
		goto func_exit;
	}

	OSScreenInit();
	screen_buf0_size = OSScreenGetBufferSizeEx(0);
	screen_buf1_size = OSScreenGetBufferSizeEx(1);
	screenBuffer = (uint8_t*)MEMBucket_alloc(screen_buf0_size+screen_buf1_size, 0x100);
	OSScreenSetBufferEx(0, (void*)(screenBuffer));
	OSScreenSetBufferEx(1, (void*)(screenBuffer + screen_buf0_size));
	OSScreenEnable(1);

	KPADInit();
	WPADEnableURCC(1);

	//garbage read
	getButtonsDown();

	ListMax = entries;
	if( ListMax > 13 )
		ListMax = 13;

	while(1)
	{
		usleep(25000);
		unsigned int btnDown = getButtonsDown();

		if( btnDown & VPAD_BUTTON_HOME )
		{
			ret = false;
			goto select_menu_end;
		}

		if( btnDown & VPAD_BUTTON_DOWN )
		{
			if( PosX + 1 >= ListMax )
			{
				if( PosX + 1 + ScrollX < entries)
					ScrollX++;
				else {
					PosX	= 0;
					ScrollX = 0;
				}
			} else {
				PosX++;
			}
			redraw = 1;
		}

		if( btnDown & VPAD_BUTTON_UP )
		{
			if( PosX <= 0 )
			{
				if( ScrollX > 0 )
					ScrollX--;
				else {
					PosX	= ListMax - 1;
					ScrollX = entries - ListMax;
				}
			} else {
				PosX--;
			}
			redraw = 1;
		}

		if( btnDown & VPAD_BUTTON_A )
			break;

		if( redraw )
		{
			OSScreenClearBuffer(0);
			OSScreenPutFont(0, 0, verChar);
			// Starting position.
			int gamelist_y = 1;
			int i;
			for (i = 0; i < ListMax; ++i, gamelist_y++)
			{
				const char *curTidChar = (const char*)(&selTitles[i+ScrollX].id);
				const char *curTnameChar = selTitles[i+ScrollX].name.c_str();
				char printStr[64];
				sprintf(printStr,"%c %s [%.4s]", i == PosX ? '>' : ' ', curTnameChar, curTidChar);
				OSScreenPutFont(0, gamelist_y, printStr);
			}
			OSScreenFlipBuffers();
			redraw = 0;
		}
	}
	//save selection
	titleToBoot = selTitles[PosX + ScrollX].id;

select_menu_end: 
	OSScreenClearBuffer(0);
	OSScreenFlipBuffers();

	OSScreenEnable(0);
	MEMBucket_free(screenBuffer);

func_exit: 
	unmount_sd_fat("sd");
	memoryRelease();

	return ret;
}

/* General Input Code */

static unsigned int wpadToVpad(unsigned int buttons)
{
	unsigned int conv_buttons = 0;

	if(buttons & WPAD_BUTTON_LEFT)
		conv_buttons |= VPAD_BUTTON_LEFT;

	if(buttons & WPAD_BUTTON_RIGHT)
		conv_buttons |= VPAD_BUTTON_RIGHT;

	if(buttons & WPAD_BUTTON_DOWN)
		conv_buttons |= VPAD_BUTTON_DOWN;

	if(buttons & WPAD_BUTTON_UP)
		conv_buttons |= VPAD_BUTTON_UP;

	if(buttons & WPAD_BUTTON_PLUS)
		conv_buttons |= VPAD_BUTTON_PLUS;

	if(buttons & WPAD_BUTTON_2)
		conv_buttons |= VPAD_BUTTON_X;

	if(buttons & WPAD_BUTTON_1)
		conv_buttons |= VPAD_BUTTON_Y;

	if(buttons & WPAD_BUTTON_B)
		conv_buttons |= VPAD_BUTTON_B;

	if(buttons & WPAD_BUTTON_A)
		conv_buttons |= VPAD_BUTTON_A;

	if(buttons & WPAD_BUTTON_MINUS)
		conv_buttons |= VPAD_BUTTON_MINUS;

	if(buttons & WPAD_BUTTON_HOME)
		conv_buttons |= VPAD_BUTTON_HOME;

	return conv_buttons;
}

static unsigned int wpadClassicToVpad(unsigned int buttons)
{
	unsigned int conv_buttons = 0;

	if(buttons & WPAD_CLASSIC_BUTTON_LEFT)
		conv_buttons |= VPAD_BUTTON_LEFT;

	if(buttons & WPAD_CLASSIC_BUTTON_RIGHT)
		conv_buttons |= VPAD_BUTTON_RIGHT;

	if(buttons & WPAD_CLASSIC_BUTTON_DOWN)
		conv_buttons |= VPAD_BUTTON_DOWN;

	if(buttons & WPAD_CLASSIC_BUTTON_UP)
		conv_buttons |= VPAD_BUTTON_UP;

	if(buttons & WPAD_CLASSIC_BUTTON_PLUS)
		conv_buttons |= VPAD_BUTTON_PLUS;

	if(buttons & WPAD_CLASSIC_BUTTON_X)
		conv_buttons |= VPAD_BUTTON_X;

	if(buttons & WPAD_CLASSIC_BUTTON_Y)
		conv_buttons |= VPAD_BUTTON_Y;

	if(buttons & WPAD_CLASSIC_BUTTON_B)
		conv_buttons |= VPAD_BUTTON_B;

	if(buttons & WPAD_CLASSIC_BUTTON_A)
		conv_buttons |= VPAD_BUTTON_A;

	if(buttons & WPAD_CLASSIC_BUTTON_MINUS)
		conv_buttons |= VPAD_BUTTON_MINUS;

	if(buttons & WPAD_CLASSIC_BUTTON_HOME)
		conv_buttons |= VPAD_BUTTON_HOME;

	if(buttons & WPAD_CLASSIC_BUTTON_ZR)
		conv_buttons |= VPAD_BUTTON_ZR;

	if(buttons & WPAD_CLASSIC_BUTTON_ZL)
		conv_buttons |= VPAD_BUTTON_ZL;

	if(buttons & WPAD_CLASSIC_BUTTON_R)
		conv_buttons |= VPAD_BUTTON_R;

	if(buttons & WPAD_CLASSIC_BUTTON_L)
		conv_buttons |= VPAD_BUTTON_L;

	return conv_buttons;
}

static unsigned int getButtonsDown()
{
	unsigned int btnDown = 0;

	int vpadError = -1;
	VPADData vpad;
	VPADRead(0, &vpad, 1, &vpadError);
	if(vpadError == 0)
		btnDown |= vpad.btns_d;

	int i;
	for(i = 0; i < 4; i++)
	{
		u32 controller_type;
		if(WPADProbe(i, &controller_type) != 0)
			continue;
		KPADData kpadData;
		KPADRead(i, &kpadData, 1);
		if(kpadData.device_type <= 1)
			btnDown |= wpadToVpad(kpadData.btns_d);
		else
			btnDown |= wpadClassicToVpad(kpadData.classic.btns_d);
	}

	return btnDown;
}
