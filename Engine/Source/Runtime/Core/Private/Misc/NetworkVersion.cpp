// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Misc/NetworkVersion.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Misc/NetworkGuid.h"
#include "HAL/IConsoleManager.h"
#include "BuildSettings.h"

DEFINE_LOG_CATEGORY( LogNetVersion );

FNetworkVersion::FGetLocalNetworkVersionOverride FNetworkVersion::GetLocalNetworkVersionOverride;
FNetworkVersion::FIsNetworkCompatibleOverride FNetworkVersion::IsNetworkCompatibleOverride;

FString FNetworkVersion::ProjectVersion;

bool FNetworkVersion::bHasCachedNetworkChecksum			= false;
uint32 FNetworkVersion::CachedNetworkChecksum			= 0;

uint32 FNetworkVersion::EngineNetworkProtocolVersion	= HISTORY_REPCMD_CHECKSUM_REMOVE_PRINTF;
uint32 FNetworkVersion::GameNetworkProtocolVersion		= 0;

uint32 FNetworkVersion::EngineCompatibleNetworkProtocolVersion		= HISTORY_REPLAY_BACKWARDS_COMPAT;
uint32 FNetworkVersion::GameCompatibleNetworkProtocolVersion		= 0;

uint32 FNetworkVersion::GetNetworkCompatibleChangelist()
{
	static int32 ReturnedVersion = ENGINE_NET_VERSION;
	static bool bStaticCheck = false;

	// add a cvar so it can be modified at runtime
	static FAutoConsoleVariableRef CVarNetworkVersionOverride(
		TEXT("networkversionoverride"), ReturnedVersion,
		TEXT("Sets network version used for multiplayer "),
		ECVF_Default);

	if (!bStaticCheck)
	{
		bStaticCheck = true;
		FParse::Value(FCommandLine::Get(), TEXT("networkversionoverride="), ReturnedVersion);
	}

	// If we have a version set explicitly, use that. Otherwise fall back to the regular engine version changelist, since it might be set at runtime (via Build.version).
	if (ReturnedVersion == 0)
	{
		return ENGINE_NET_VERSION ? ENGINE_NET_VERSION : BuildSettings::GetCompatibleChangelist();
	}

	return (uint32)ReturnedVersion;
}

uint32 FNetworkVersion::GetReplayCompatibleChangelist()
{
	return BuildSettings::GetCurrentChangelist();
}

uint32 FNetworkVersion::GetEngineNetworkProtocolVersion()
{
	return EngineNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetEngineCompatibleNetworkProtocolVersion()
{
	return EngineCompatibleNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetGameNetworkProtocolVersion()
{
	return GameNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetGameCompatibleNetworkProtocolVersion()
{
	return GameCompatibleNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetLocalNetworkVersion( bool AllowOverrideDelegate /*=true*/ )
{
	if ( bHasCachedNetworkChecksum )
	{
		return CachedNetworkChecksum;
	}

	if ( AllowOverrideDelegate && GetLocalNetworkVersionOverride.IsBound() )
	{
		CachedNetworkChecksum = GetLocalNetworkVersionOverride.Execute();

		UE_LOG( LogNetVersion, Log, TEXT( "Checksum from delegate: %u" ), CachedNetworkChecksum );

		bHasCachedNetworkChecksum = true;

		return CachedNetworkChecksum;
	}

	FString VersionString = FString::Printf(TEXT("%s %s, NetCL: %d, EngineNetVer: %d, GameNetVer: %d"),
		FApp::GetProjectName(),
		*ProjectVersion,
		GetNetworkCompatibleChangelist(),
		FNetworkVersion::GetEngineNetworkProtocolVersion(),
		FNetworkVersion::GetGameNetworkProtocolVersion());

	CachedNetworkChecksum = FCrc::StrCrc32(*VersionString.ToLower());

	UE_LOG(LogNetVersion, Log, TEXT("%s (Checksum: %u)"), *VersionString, CachedNetworkChecksum);

	bHasCachedNetworkChecksum = true;

	return CachedNetworkChecksum;
}

bool FNetworkVersion::IsNetworkCompatible( const uint32 LocalNetworkVersion, const uint32 RemoteNetworkVersion )
{
	if ( IsNetworkCompatibleOverride.IsBound() )
	{
		return IsNetworkCompatibleOverride.Execute( LocalNetworkVersion, RemoteNetworkVersion );
	}

	return LocalNetworkVersion == RemoteNetworkVersion;
}

FNetworkReplayVersion FNetworkVersion::GetReplayVersion()
{
	const uint32 ReplayVersion = ( GameCompatibleNetworkProtocolVersion << 16 ) | EngineCompatibleNetworkProtocolVersion;

	return FNetworkReplayVersion( FApp::GetProjectName(), ReplayVersion, GetReplayCompatibleChangelist() );
}
