// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/Trace.h"
#include "Trace/Analyzer.h"
#include "Templates/SharedPointer.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Common/PagedArray.h"

namespace Trace
{
	class IAnalysisSession;
	class FThreadProvider;
	class FBookmarkProvider;
	class FLogProvider;
	class FFrameProvider;
}

class FMiscTraceAnalyzer
	: public Trace::IAnalyzer
{
public:
	FMiscTraceAnalyzer(Trace::IAnalysisSession& Session,
					   Trace::FThreadProvider& ThreadProvider,
					   Trace::FBookmarkProvider& BookmarkProvider,
					   Trace::FLogProvider& LogProvider,
					   Trace::FFrameProvider& FrameProvider);
	virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	virtual void OnEvent(uint16 RouteId, const FOnEventContext& Context) override;
	virtual void OnAnalysisEnd() override {};

private:
	enum : uint16
	{
		RouteId_RegisterGameThread,
		RouteId_CreateThread,
		RouteId_SetThreadGroup,
		RouteId_BeginThreadGroupScope,
		RouteId_EndThreadGroupScope,
		RouteId_BookmarkSpec,
		RouteId_Bookmark,
		RouteId_BeginFrame,
		RouteId_EndFrame,
		RouteId_BeginGameFrame,
		RouteId_EndGameFrame,
		RouteId_BeginRenderFrame,
		RouteId_EndRenderFrame,
	};

	struct FThreadState
	{
		TArray<const TCHAR*> ThreadGroupStack;
	};

	FThreadState* GetThreadState(uint32 ThreadId);

	Trace::IAnalysisSession& Session;
	Trace::FThreadProvider& ThreadProvider;
	Trace::FBookmarkProvider& BookmarkProvider;
	Trace::FLogProvider& LogProvider;
	Trace::FFrameProvider& FrameProvider;
	TMap<uint32, TSharedRef<FThreadState>> ThreadStateMap;
	uint64 LastFrameCycle[TraceFrameType_Count] = { 0, 0 };
};

