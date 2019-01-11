// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WebRTCProxyTarget : TargetRules
{
	public WebRTCProxyTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LaunchModuleName = "WebRTCProxy";

        GlobalDefinitions.Add("WEBRTC_WIN");
		GlobalDefinitions.Add("INCL_EXTRA_HTON_FUNCTIONS");

		// Lean and mean
		bCompileLeanAndMeanUE = true;
		
        // No editor needed
        bBuildEditor = false;
		bBuildWithEditorOnlyData = false;

		// Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = false;

		// Console application, not a Windows app (sets entry point to main(), instead of WinMain())
		bIsBuildingConsoleApplication = true;
    }
}