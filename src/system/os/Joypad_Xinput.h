#pragma once
#include "os/Joypad.h"
#include "xdk/XAPILIB.h"

void JoypadInitXboxPCDeadzone(class DataArray *);
void TranslateStick(char *, short, bool, bool);
void TranslateButtons(unsigned int *, unsigned short);
bool JoypadGetCachedXInputCaps(int, XINPUT_CAPABILITIES *, bool);

JoypadType ReadSingleXinputJoypad(
    int,
    int,
    unsigned int *,
    char *,
    char *,
    char *,
    char *,
    char *,
    char *,
    float *const,
    float *const,
    unsigned char *const
);

void JoypadResetXboxPC(int);
// Retail 0x82532378 / 0x82531FF8 / 0x82532030. The names are ours.
void JoypadSetXinputActuators(int pad, int left, int right);
void JoypadSetXinputCalbertMode(int pad, int mode);
void JoypadInvalidateXinputCaps(int pad);
