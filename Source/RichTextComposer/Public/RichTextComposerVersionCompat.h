#pragma once

#include "CoreMinimal.h"
#include "Misc/EngineVersionComparison.h"

/**
 * 引擎版本判断（多版本兼容用）。
 *
 * 官方 UE_VERSION_OLDER_THAN 在不同引擎版本间**参数个数不同**
 * （5.7 起变成 Major/Minor/Patch 三元组，更早是二元组），
 * 直接在源码里用会导致跨版本编译失败，因此这里基于
 * ENGINE_MAJOR_VERSION / ENGINE_MINOR_VERSION 自定义一个通用版本。
 *
 *   #if RTC_ENGINE_OLDER_THAN(5, 7)
 *   ...
 *   #endif
 */
#define RTC_ENGINE_OLDER_THAN(Major, Minor) \
	((ENGINE_MAJOR_VERSION < (Major)) || (ENGINE_MAJOR_VERSION == (Major) && ENGINE_MINOR_VERSION < (Minor)))
