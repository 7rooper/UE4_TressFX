// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "OSCServer.h"

#include "Runtime/Core/Public/Async/TaskGraphInterfaces.h"
#include "Sockets.h"
#include "Common/UdpSocketBuilder.h"
#include "Common/UdpSocketReceiver.h"

#include "OSCStream.h"
#include "OSCMessage.h"
#include "OSCMessagePacket.h"
#include "OSCBundle.h"
#include "OSCBundlePacket.h"
#include "OSCLog.h"

#define OSC_SERVER_BUFFER_FREE 0
#define OSC_SERVER_BUFFER_LOCKED 1


UOSCServer::UOSCServer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) 
	, bWhitelistClients(false)
	, Socket(nullptr)
	, SocketReceiver(nullptr)
	, OSCBufferLock(0)
	, Port(0)
	, bMulticastLoopback(false)
{
}

void UOSCServer::Callback(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint)
{
	// Throw request on the ground if not endpoint address not whitelisted.
	if (bWhitelistClients && !ClientWhitelist.Contains(Endpoint.Address.Value))
	{
		return;
	}

	TSharedPtr<IOSCPacket> Packet = IOSCPacket::CreatePacket(Data->GetData());
	if (Packet.IsValid())
	{
		FOSCStream Stream = FOSCStream(Data->GetData(), Data->Num());

		Packet->ReadData(Stream);
		OSCPackets.Enqueue(MoveTemp(Packet));
	}

	if (!FPlatformAtomics::InterlockedCompareExchange(&OSCBufferLock, OSC_SERVER_BUFFER_LOCKED, OSC_SERVER_BUFFER_FREE) &&
		!OSCPackets.IsEmpty())
	{
		check(OSCBufferLock == OSC_SERVER_BUFFER_LOCKED);
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(FSimpleDelegateGraphTask::FDelegate::CreateLambda([this]()
		{
			check(OSCBufferLock == OSC_SERVER_BUFFER_LOCKED);
			FPlatformAtomics::InterlockedCompareExchange(&OSCBufferLock, OSC_SERVER_BUFFER_FREE, OSC_SERVER_BUFFER_LOCKED);
				
			TSharedPtr<IOSCPacket> Packet;
			while (OSCPackets.Dequeue(Packet))
			{
				if (Packet->IsMessage())
				{
					FOSCMessage Message(FOSCMessage(StaticCastSharedPtr<FOSCMessagePacket>(Packet)));
					OnOscReceived.Broadcast(Message);

					for (const TPair<uint32, FOSCAddress>& Pair: AddressPatterns)
					{
						if (Pair.Value.Matches(Message.GetAddress()))
						{
							OnOscDispatched.Broadcast(Pair.Value, Message);
						}
					}
				}
				else if (Packet->IsBundle())
				{
					FOSCBundle Bundle(StaticCastSharedPtr<FOSCBundlePacket>(Packet));
					OnOscBundleReceived.Broadcast(Bundle);
				}
				else
				{
					UE_LOG(LogOSC, Warning, TEXT("Failed to parse invalid received OSC message. OSCAddress '%s' is invalid."), *Packet->GetAddress().GetFullPath());
				}
			}
		}),
		TStatId(),
		nullptr,
		ENamedThreads::GameThread);
	}
}

bool UOSCServer::GetMulticastLoopback() const
{
	return bMulticastLoopback;
}

bool UOSCServer::IsActive() const
{
	return SocketReceiver != nullptr;
}

void UOSCServer::Listen()
{
	if (IsActive())
	{
		UE_LOG(LogOSC, Error, TEXT("OSCServer currently listening: %s:%d. Failed to start new service prior to calling stop."), *GetName(), *ReceiveIPAddress.ToString(), Port);
		return;
	}

	FUdpSocketBuilder Builder(*GetName());
	Builder.BoundToPort(Port);
	if (ReceiveIPAddress.IsMulticastAddress())
	{
		Builder.JoinedToGroup(ReceiveIPAddress);
		if (bMulticastLoopback)
		{
			Builder.WithMulticastLoopback();
		}
	}
	else
	{
		if (bMulticastLoopback)
		{
			UE_LOG(LogOSC, Warning, TEXT("OSCServer '%s' ReceiveIPAddress provided is not a multicast address.  Not respecting MulticastLoopback boolean."), *GetName());
		}
		Builder.BoundToAddress(ReceiveIPAddress);
	}

	Socket = Builder.Build();
	if (Socket)
	{
		SocketReceiver = new FUdpSocketReceiver(Socket, FTimespan::FromMilliseconds(100), *GetName().Append(TEXT("_ListenerThread")));
		SocketReceiver->OnDataReceived().BindUObject(this, &UOSCServer::Callback);
		SocketReceiver->Start();

		UE_LOG(LogOSC, Display, TEXT("OSCServer '%s' Listening: %s:%d."), *GetName(), *ReceiveIPAddress.ToString(), Port);
	}
	else
	{
		UE_LOG(LogOSC, Warning, TEXT("OSCServer '%s' failed to bind to socket on %s:%d."), *GetName(), *ReceiveIPAddress.ToString(), Port);
	}
}

bool UOSCServer::SetAddress(const FString& InReceiveIPAddress, int32 InPort)
{
	if (IsActive())
	{
		UE_LOG(LogOSC, Error, TEXT("Cannot set address while OSCServer is active."));
		return false;
	}

	if (!FIPv4Address::Parse(InReceiveIPAddress, ReceiveIPAddress))
	{
		UE_LOG(LogOSC, Error, TEXT("Invalid ReceiveIPAddress when attempting to set on OSCServer '%s'. Address not updated."), *GetName());
		return false;
	}

	Port = InPort;
	return true;
}

void UOSCServer::SetMulticastLoopback(bool bInMulticastLoopback)
{
	if (bInMulticastLoopback != bMulticastLoopback && IsActive())
	{
		UE_LOG(LogOSC, Error, TEXT("Cannot update MulticastLoopback while OSCServer '%s' is active."), *GetName());
		return;
	}

	bMulticastLoopback = bInMulticastLoopback;
}

void UOSCServer::Stop()
{
	if (SocketReceiver)
	{
		delete SocketReceiver;
		SocketReceiver = nullptr;
	}

	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
}

void UOSCServer::BeginDestroy()
{
	Stop();
	Super::BeginDestroy();
}

void UOSCServer::AddWhitelistedClient(const FString& IPAddress)
{
	FIPv4Address OutAddress;
	if (!FIPv4Address::Parse(IPAddress, OutAddress))
	{
		UE_LOG(LogOSC, Warning, TEXT("OSCServer '%s' failed to whitelist IP Address '%s'. Address is invalid."), *IPAddress);
		return;
	}

	ClientWhitelist.Add(OutAddress.Value);
}

void UOSCServer::RemoveWhitelistedClient(const FString& IPAddress)
{
	FIPv4Address OutAddress;
	if (!FIPv4Address::Parse(IPAddress, OutAddress))
	{
		UE_LOG(LogOSC, Warning, TEXT("OSCServer '%s' failed to remove whitelisted IP Address '%s'. Address is invalid."), *IPAddress);
		return;
	}

	ClientWhitelist.Remove(OutAddress.Value);
}

void UOSCServer::ClearWhitelistedClients()
{
	ClientWhitelist.Reset();
}

TSet<FString> UOSCServer::GetWhitelistedClients() const
{
	TSet<FString> OutWhitelist;
	for (uint32 Client : ClientWhitelist)
	{
		OutWhitelist.Add(FIPv4Address(Client).ToString());
	}

	return OutWhitelist;
}

void UOSCServer::AddOSCAddressPattern(const FOSCAddress& OSCAddressPattern)
{
	if (OSCAddressPattern.IsValidPattern())
	{
		AddressPatterns.Add(OSCAddressPattern.GetHash(), OSCAddressPattern);
	}
}

void UOSCServer::RemoveOSCAddressPattern(const FOSCAddress& OSCAddressPattern)
{
	if (OSCAddressPattern.IsValidPattern())
	{
		AddressPatterns.Remove(OSCAddressPattern.GetHash());
	}
}


TArray<FOSCAddress> UOSCServer::GetOSCAddressPatterns() const
{
	TArray<FOSCAddress> Addresses;
	for (const TPair<uint32, FOSCAddress>& Pair : AddressPatterns)
	{
		Addresses.Add(Pair.Value);
	}
	return MoveTemp(Addresses);
}

void UOSCServer::ClearOSCAddressPatterns()
{
	AddressPatterns.Reset();
}