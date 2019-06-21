// Copyright 2018 Sean Chen. All Rights Reserved.

using UnrealBuildTool;

public class TressFXEditor : ModuleRules
{
    public TressFXEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        //Definitions.Add("TRESSFX_SINGLE_PLUGIN=1");

        bFasterWithoutUnity = true;
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
				// ... add public include paths required here ...
			}
            );


        PrivateIncludePaths.AddRange(
            new string[] {
                "TressFXEditor/Private",
				// ... add other private include paths required here ...
			}
            );


        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "TressFX"

				// ... add other public dependencies that you statically link with here ...
			}
            );


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "UnrealEd",
                "EditorStyle",
                "Slate",
                "SlateCore",
                "MainFrame",
                "TressFX",
                "Json",
                "JsonUtilities",
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
