// SPDX-FileCopyrightText: 2026 PullsarDev
// SPDX-License-Identifier: Apache-2.0

// The only UBT translation unit compiling the vendored implementation. ThirdParty
// lives outside Source so UBT cannot also discover and compile its .cpp files.
// Intentionally compile under the real Unreal macros/PCH, including in unity builds.
#include "../../../ThirdParty/InterestEngine/src/step.cpp"
#include "../../../ThirdParty/InterestEngine/src/version.cpp"

#if defined(_MSC_VER) && !defined(__clang__) && !defined(_M_FP_PRECISE)
#error "The Interest Engine kernel requires /fp:precise."
#endif
