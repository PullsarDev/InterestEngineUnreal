// SPDX-FileCopyrightText: 2026 PullsarDev
// SPDX-License-Identifier: Apache-2.0

using System.IO;
using UnrealBuildTool;

public class InterestEngineRuntime : ModuleRules
{
	public InterestEngineRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Keep the module FP override and its PCH in the same compile environment.
		// Shared engine PCHs may have different flags, especially for Game targets.
		PrivatePCHHeaderFile = "Private/InterestEngineRuntimePCH.h";
		FPSemantics = FPSemanticsMode.Precise;
		CppStandard = CppStandardVersion.Cpp20;
		PublicDependencyModuleNames.Add("Core");
		PublicSystemIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory,
			"../../ThirdParty/InterestEngine/include")));
	}
}
