// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AjaMediaOutput : ModuleRules
	{
		public AjaMediaOutput(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"AjaMedia"
				});

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"AjaMediaOutput/Private",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AJA",
					"Core",
					"CoreUObject",
					"Engine",
                    "MediaIOCore",
                    "MovieSceneCapture",
					"RenderCore",
					"RHI",
					"Slate",
					"SlateCore",
                    "TimeManagement",
				}
			);

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("UnrealEd");
            }
		}
	}
}
