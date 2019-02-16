// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "GenericPlatform/GenericPlatformBackgroundHttpRequest.h"
#include "GenericPlatform/GenericPlatformBackgroundHttpResponse.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IHttpRequest.h"
#include "HttpModule.h"
#include "BackgroundHttpModule.h"

FGenericPlatformBackgroundHttpRequest::FGenericPlatformBackgroundHttpRequest()
	: RequestWrapper(nullptr)
{
}

bool FGenericPlatformBackgroundHttpRequest::HandleDelayedProcess()
{
	//Our request may already have a response result from associating with another already completed request. If so, lets not spin up our RequestWrapper.
	if (!GetResponse().IsValid())
	{
		if (!RequestWrapper.IsValid())
		{
			RequestWrapper = MakeShareable<FGenericPlatformBackgroundHttpWrapper>(new FGenericPlatformBackgroundHttpWrapper(SharedThis(this), NumberOfTotalRetries));
		}

		if (ensureAlwaysMsgf(RequestWrapper.IsValid(), TEXT("Failure creating FGenericPlatformBackgroundHttpWrapper! Can not process BackgroundHttp request! RequestID:%s"), *GetRequestID()))
		{
			//Start request wrapper
			RequestWrapper->MakeRequest();
		}
	}

	return RequestWrapper.IsValid() || GetResponse().IsValid();
}

void FGenericPlatformBackgroundHttpRequest::CancelRequest()
{
	FBackgroundHttpRequestImpl::CancelRequest();
	RequestWrapper.Reset();
}

void FGenericPlatformBackgroundHttpRequest::FGenericPlatformBackgroundHttpRequest::CompleteWithExistingResponseData(FBackgroundHttpResponsePtr BackgroundResponse)
{
	//Remove our request wrapper immediately so the Response cleans up ASAP instead of the user having to delete this task to clean up the Http Response.
	RequestWrapper.Reset();

	FBackgroundHttpRequestImpl::CompleteWithExistingResponseData(BackgroundResponse);
}

FGenericPlatformBackgroundHttpRequest::FGenericPlatformBackgroundHttpWrapper::FGenericPlatformBackgroundHttpWrapper(FBackgroundHttpRequestPtr Request, int MaxRetriesToAttempt)
	: OriginalRequest(Request)
	, CurrentRetryNumber(0)
	, MaxRetries(MaxRetriesToAttempt)
	, LastProgressUpdateBytes(0)
{
}

FGenericPlatformBackgroundHttpRequest::FGenericPlatformBackgroundHttpWrapper::~FGenericPlatformBackgroundHttpWrapper()
{
	CleanUpHttpRequest();
}

void FGenericPlatformBackgroundHttpRequest::FGenericPlatformBackgroundHttpWrapper::MakeRequest()
{
	if (ensureAlwaysMsgf(OriginalRequest.IsValid(), TEXT("Call to MakeRequest in BackgroundHttpWrapper without associating it to a valid BackgroundHttpRequest! Should never happen!")))
	{
		//If we have an existing HttpRequest, lets clear it out before creating a new one
		CleanUpHttpRequest();

		//Create new Request
		HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->OnProcessRequestComplete().BindRaw(this, &FGenericPlatformBackgroundHttpRequest::FGenericPlatformBackgroundHttpWrapper::HttpRequestComplete);
		HttpRequest->OnRequestProgress().BindRaw(this, &FGenericPlatformBackgroundHttpRequest::FGenericPlatformBackgroundHttpWrapper::UpdateHttpProgress);

		const FString& RequestURL = GetURLForCurrentRetryNumber();
		if (!RequestURL.IsEmpty())
		{
			HttpRequest->SetURL(RequestURL);
			HttpRequest->ProcessRequest();
		}
		else
		{
			UE_LOG(LogBackgroundHttpRequest, Display, TEXT("No valid URL for Request. (No valid URL, or out of Retries) - RequestID:%s | RetryNumber:%d | MaxRetries:%d"), *OriginalRequest->GetRequestID(), CurrentRetryNumber, MaxRetries);

			//We don't have a valid URL, so just error immediately
			HttpRequestComplete(HttpRequest, nullptr, false);
		}
	}
}

void FGenericPlatformBackgroundHttpRequest::FGenericPlatformBackgroundHttpWrapper::CleanUpHttpRequest()
{
	if (HttpRequest.IsValid())
	{
		HttpRequest->OnProcessRequestComplete().Unbind();
		HttpRequest->OnRequestProgress().Unbind();
		HttpRequest->CancelRequest();

		HttpRequest.Reset();
	}
}

const FString FGenericPlatformBackgroundHttpRequest::FGenericPlatformBackgroundHttpWrapper::GetURLForCurrentRetryNumber()
{
	FString ReturnedURL = FString();

	int URLIndexToUse = CurrentRetryNumber;

	//Don't find a valid URL if we are already passed our retry count
	if (URLIndexToUse <= MaxRetries)
	{
		const TArray<FString>& URLList = OriginalRequest->GetURLList();
		if (URLList.Num() > 0)
		{
			//If our number is too high, lets loop back around to the start of the list
			while ((URLIndexToUse >= 0) && (!URLList.IsValidIndex(URLIndexToUse)))
			{
				URLIndexToUse -= URLList.Num();
			}
		}

		if (URLList.IsValidIndex(URLIndexToUse))
		{
			ReturnedURL = URLList[URLIndexToUse];
		}
	}

	return ReturnedURL;
}

void FGenericPlatformBackgroundHttpRequest::FGenericPlatformBackgroundHttpWrapper::HttpRequestComplete(FHttpRequestPtr HttpRequestIn, FHttpResponsePtr HttpResponse, bool bSuccess)
{
	//Create a new FGenericPlatformBackgroundHttpRequest from the HttpRequest/Response
	FBackgroundHttpResponsePtr ConstructedResponse = MakeShareable(new FGenericPlatformBackgroundHttpResponse(HttpRequestIn, HttpResponse, bSuccess));

	//Let the FGenericPlatformBackgroundHttpResponse drive if this succeeded or not
	const bool bRequestWasSuccessful = EHttpResponseCodes::IsOk(ConstructedResponse->GetResponseCode());

	UE_LOG(LogBackgroundHttpRequest, Display, TEXT("Underlying HttpRequest complete - RequestID:%s | bRequestWasSuccessfull:%d | CurrentRetryNumber:%d | MaxRetries:%d"), *OriginalRequest->GetRequestID(), (int)(bRequestWasSuccessful), CurrentRetryNumber, MaxRetries);

	//Attempt a retry on failure
	if (!bRequestWasSuccessful)
	{
		++CurrentRetryNumber;
		MakeRequest();
	}
	else
	{
		//Pass constructed response to our BackgroundRequest so it can complete itself using this response
		OriginalRequest->CompleteWithExistingResponseData(ConstructedResponse);
	}
}

void FGenericPlatformBackgroundHttpRequest::FGenericPlatformBackgroundHttpWrapper::UpdateHttpProgress(FHttpRequestPtr UnderlyingHttpRequest, int32 BytesSent, int32 BytesReceived)
{
	const int32 ByteDifference = (LastProgressUpdateBytes > 0) ? LastProgressUpdateBytes - BytesReceived : BytesReceived;
	LastProgressUpdateBytes = ByteDifference;
	
	UE_LOG(LogBackgroundHttpRequest, VeryVerbose, TEXT("HttpRequest Progress Update- RequestID:%s | BytesSent: %d | BytesReceived:%d | BytesReceivedSinceLastUpdate:%d"), *OriginalRequest->GetRequestID(), BytesSent, BytesReceived, ByteDifference);
	
	OriginalRequest->OnProgressUpdated().ExecuteIfBound(OriginalRequest, BytesReceived, LastProgressUpdateBytes);
}
