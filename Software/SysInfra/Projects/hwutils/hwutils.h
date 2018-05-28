#pragma once

#if defined(__linux__)

static const char HwUtils_BinName[] = "hwutils";
static const char HwUtils_GetSystemSerialArg[] = "--system-serial";
static const char HwUtils_GetHddPrimarySerialArg[] = "--hdd-serial";

const int HwUtilsRetOk      = 0;
const int HwUtilsRetBadArgs = 1;
const int HwUtilsRetError   = 2;

#endif
