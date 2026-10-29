// SPDX-FileCopyrightText: 2026 PullsarDev
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreMinimal.h"

#if defined(_MSC_VER) && !defined(__clang__) && !defined(_M_FP_PRECISE)
#error "InterestEngineRuntime requires /fp:precise. Check the module's private PCH and FPSemantics."
#endif
