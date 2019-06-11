// Copyright (c) Microsoft Corporation. All rights reserved.

using System;
using System.IO;
using UnrealBuildTool;
using System.Collections.Generic;
using Microsoft.Win32;
using System.Diagnostics;


namespace UnrealBuildTool.Rules
{
	public class WindowsMixedRealityHMD : ModuleRules
	{
		private string ModulePath
		{
			get { return ModuleDirectory; }
		}
	 
		private void LoadMixedReality(ReadOnlyTargetRules Target)
        {
            string DllSubpath = "undefined";

            if (Target.WindowsPlatform.Architecture == WindowsArchitecture.x86)
            {
                DllSubpath = "Win32";
            }
            else if (Target.WindowsPlatform.Architecture == WindowsArchitecture.x64)
            {
                DllSubpath = "Win64";
            }

            if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
            {
				// Set a macro allowing us to switch between debuggame/development configuration
				if (Target.Configuration == UnrealTargetConfiguration.Debug)
				{
					PrivateDefinitions.Add("WINDOWS_MIXED_REALITY_DEBUG_DLL=1");
				}
				else
				{
					PrivateDefinitions.Add("WINDOWS_MIXED_REALITY_DEBUG_DLL=0");
				}

				PublicDelayLoadDLLs.Add("HolographicAppRemoting.dll");
                PublicDelayLoadDLLs.Add("PerceptionDevice.dll");
				RuntimeDependencies.Add(String.Format("$(EngineDir)/Binaries/ThirdParty/Windows/HoloLens/{0}/HolographicAppRemoting.dll", DllSubpath));
                RuntimeDependencies.Add(String.Format("$(EngineDir)/Binaries/ThirdParty/Windows/HoloLens/{0}/PerceptionDevice.dll", DllSubpath));
            }

            PublicDefinitions.Add("WITH_WINDOWS_MIXED_REALITY=1");
        }
        
        public WindowsMixedRealityHMD(ReadOnlyTargetRules Target) : base(Target)
		{
			bEnableExceptions = true;

			if (Target.Platform == UnrealTargetPlatform.Win32 ||
				Target.Platform == UnrealTargetPlatform.Win64 ||
				Target.Platform == UnrealTargetPlatform.HoloLens)
			{
				PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"HeadMountedDisplay",
						"ProceduralMeshComponent",
                        "MixedRealityInteropLibrary",
                        "InputDevice",
					}
				);
			
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"Core",
						"CoreUObject",
                        "ApplicationCore",
						"Engine",
						"InputCore",
						"RHI",
						"RenderCore",
						"Renderer",
						"HeadMountedDisplay",
						"D3D11RHI",
						"Slate",
						"SlateCore",
						"UtilityShaders",
						"Projects",
                        "WindowsMixedRealityHandTracking",
					}
					);

				if (Target.bBuildEditor == true)
				{
					PrivateDependencyModuleNames.Add("UnrealEd");
				}

				if (Target.Platform != UnrealTargetPlatform.HoloLens)
				{
					AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
					AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelMetricsDiscovery");
				}

                AddEngineThirdPartyPrivateStaticDependencies(Target, "WindowsMixedRealityInterop");

                LoadMixedReality(Target);

                PrivateIncludePaths.AddRange(
					new string[]
					{
					"WindowsMixedRealityHMD/Private",
					"../../../../Source/Runtime/Windows/D3D11RHI/Private",
					"../../../../Source/Runtime/Renderer/Private",
					});

				if (Target.Platform == UnrealTargetPlatform.Win32 ||
					Target.Platform == UnrealTargetPlatform.Win64)
				{
					PrivateIncludePaths.Add("../../../../Source/Runtime/Windows/D3D11RHI/Private/Windows");
				}
				else if (Target.Platform == UnrealTargetPlatform.HoloLens)
				{
					PrivateIncludePaths.Add("../../../../Source/Runtime/Windows/D3D11RHI/Private/HoloLens");
                    PrivateDependencyModuleNames.Add("HoloLensAR");
				}

                bFasterWithoutUnity = true;

				PCHUsage = PCHUsageMode.NoSharedPCHs;
				PrivatePCHHeaderFile = "Private/WindowsMixedRealityPrecompiled.h";
			}
		}
	}
}
