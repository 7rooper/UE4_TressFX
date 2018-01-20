// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ProfilerMessages : ModuleRules
	{
		public ProfilerMessages(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				});

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"Editor/ProfilerMessages/Private",
				});

			if (Target.Configuration != UnrealTargetConfiguration.Shipping)
			{
				PrecompileForTargets = PrecompileTargetsType.Any;
			}
		}
	}
}
