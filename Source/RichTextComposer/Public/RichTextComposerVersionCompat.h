#pragma once

#include "CoreMinimal.h"
#include "Misc/EngineVersionComparison.h"

/**
 * Engine version checks (used for multi-version compatibility).
 *
 * The official UE_VERSION_OLDER_THAN takes a **different number of arguments** across versions
 * (three of them - Major/Minor/Patch - since 5.7, two before that),
 * so using it directly breaks cross-version builds. This macro is therefore defined on top of
 * ENGINE_MAJOR_VERSION / ENGINE_MINOR_VERSION instead.
 *
 *   #if RTC_ENGINE_OLDER_THAN(5, 7)
 *   ...
 *   #endif
 */
#define RTC_ENGINE_OLDER_THAN(Major, Minor) \
	((ENGINE_MAJOR_VERSION < (Major)) || (ENGINE_MAJOR_VERSION == (Major) && ENGINE_MINOR_VERSION < (Minor)))
