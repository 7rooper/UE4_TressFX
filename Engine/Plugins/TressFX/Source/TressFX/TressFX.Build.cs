// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class TressFX : ModuleRules
{
	public TressFX(ReadOnlyTargetRules Target) : base(Target)
	{
        // Definitions.Add("TRESSFX_STANDALONE_PLUGIN=1");
		PrivateIncludePaths.Add("../../../Shaders/Shared");

        //OptimizeCode = CodeOptimization.Never; //uncomment this when debugging
        bFasterWithoutUnity = true;
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
                }
			);
				
		
		PrivateIncludePaths.AddRange(
				new string[] {
					"TressFX/Private"
					// ... add other private include paths required here ...
				}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "RenderCore",
                "RHI",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
            {
                "CoreUObject",
                "Engine",
                "Projects",
                "Renderer",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}