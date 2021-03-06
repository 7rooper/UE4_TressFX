// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CoreMinimal.h"

//Call backs called by the bellow FBackgroundURLSessionHandler so higher-level systems can respond to task updates.
class APPLICATIONCORE_API FIOSBackgroundDownloadCoreDelegates
{
public:
    DECLARE_MULTICAST_DELEGATE_ThreeParams(FIOSBackgroundDownload_DidFinishDownloadingToURL, NSURLSessionDownloadTask*, NSError*, const FString&);
    DECLARE_MULTICAST_DELEGATE_FourParams(FIOSBackgroundDownload_DidWriteData, NSURLSessionDownloadTask*, int64_t /*Bytes Written Since Last Call */, int64_t /*Total Bytes Written */, int64_t /*Total Bytes Expedted To Write */);
    DECLARE_MULTICAST_DELEGATE_TwoParams(FIOSBackgroundDownload_DidCompleteWithError, NSURLSessionTask*, NSError*);
    DECLARE_MULTICAST_DELEGATE_OneParam(FIOSBackgroundDownload_SessionDidFinishAllEvents, NSURLSession*);
    
	static FIOSBackgroundDownload_DidFinishDownloadingToURL OnIOSBackgroundDownload_DidFinishDownloadingToURL;
	static FIOSBackgroundDownload_DidWriteData OnIOSBackgroundDownload_DidWriteData;
	static FIOSBackgroundDownload_DidCompleteWithError OnIOSBackgroundDownload_DidCompleteWithError;
	static FIOSBackgroundDownload_SessionDidFinishAllEvents OnIOSBackgroundDownload_SessionDidFinishAllEvents;
};

//Interface for wrapping a NSURLSession configured to support background downloading of NSURLSessionDownloadTasks.
//This exists here as we can have to re-associate with our background session after app launch and need to re-associate with downloads
//right away before the HttpModule is loaded.
class APPLICATIONCORE_API FBackgroundURLSessionHandler
{
public:
	// Initializes a BackgroundSession with the given identifier. If the current background session already exists, returns true if the identifier matches. False if identifier doesn't match or if the session fails to create.
	static bool InitBackgroundSession(const FString& SessionIdentifier);

	//bShouldInvalidateExistingTasks determines if the session cancells all outstanding tasks immediately and cancels the session immediately or waits for them to finish and then invalidates the session
	static void ShutdownBackgroundSession(bool bShouldFinishTasksFirst = true);

	//Gets a pointer to the current background session
	static NSURLSession* GetBackgroundSession();

	//Gets a path stored in an FString of the working directory any completed temp files will be downloaded
	static const FString& GetBackgroundSessionWorkingDirectoryPath();

	/**
	* Function that takes in a URL and figures out the location we should use as the temp storage URL
	*
	* @return FString to use as the TempFilePath
	*/
	static const FString GetTemporaryFilePathFromURL(const FString& URL);

	static void CreateBackgroundSessionWorkingDirectory();
private:
	static NSURLSession* BackgroundSession;
	static FString CachedIdentifierName;
};


//Delegate object associated with our above NSURLSession
@interface BackgroundDownloadDelegate : NSObject<NSURLSessionDownloadDelegate>
@end
