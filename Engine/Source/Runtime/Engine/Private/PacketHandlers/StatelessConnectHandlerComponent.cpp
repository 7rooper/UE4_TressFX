// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PacketHandlers/StatelessConnectHandlerComponent.h"
#include "Stats/Stats.h"
#include "Serialization/MemoryWriter.h"
#include "EngineStats.h"
#include "Misc/SecureHash.h"
#include "Engine/NetConnection.h"
#include "PacketAudit.h"




DEFINE_LOG_CATEGORY(LogHandshake);



// @todo #JohnB: It is important that banning functionality gets implemented here. This is the earliest/best place,
//					to reject connections (however, format of 'Address', is defined by net driver - making this tricky, e.g. Steam)

// @todo #JohnB: Do profiling of the handshake check, plus the entire PacketHandler 'Incoming'/'Outgoing' chain,
//					when under a DoS attack.

// @todo #JohnB: Consider adding an increasing-cost challenge at some stage. Not strictly necessary, but may be nice to have.


/**
 * Purpose:
 *
 * UDP connections are vulnerable to various types of DoS attacks, particularly spoofing the IP address in UDP packets,
 * and to protect against this a handshake is needed to verify that the IP is really owned by the client.
 *
 * This handshake can be implemented in two ways:
 *	Stateful:	Here the server stores connection state information (i.e. maintains a UNetConnection) while the handshake is underway,
 *				allowing spoofed packets to allocate server memory space, prior to handshake verification.
 *
 *	Stateless:	Here the server does not store any connection state information, until the handshake completes,
 *				preventing spoofed packets from allocating server memory space until after the handshake.
 *
 * Stateful handshakes are vulnerable to DoS attacks through server memory usage, whereas stateless handshakes are not,
 * so this implementation uses stateless handshakes.
 *
 *
 *
 * Handshake Process/Protocol:
 *
 * The protocol for the handshake involves the client sending an initial packet to the server,
 * and the server responding with a unique 'Cookie' value, which the client has to respond with.
 *
 * Client - Initial Connect:
 * [HandshakeBit][SecretIdBit][24:PacketSizeFiller][AlignPad]
 *																--->
 *																		Server - Stateless Handshake Challenge:
 *																		[HandshakeBit][SecretIdBit][4:Timestamp][20:Cookie][AlignPad]
 *																<---
 * Client - Stateless Challenge Response:
 * [HandshakeBit][SecretIdBit][4:Timestamp][20:Cookie][AlignPad]
 *																--->
 *																		Server:
 *																		Ignore, or create UNetConnection.
 *
 *																		Server - Stateless Handshake Ack
 *																		[HandshakeBit][SecretIdBit][4:Timestamp][20:Cookie][AlignPad]
 *																<---
 * Client:
 * Handshake Complete.
 *
 *
 *	- HandshakeBit:		Bit signifying whether a packet is a handshake packet. Applied to all game packets.
 *	- SecretIdBit:		For handshake packets, specifies which HandshakeSecret array was used to generate Cookie.
 *	- Timestamp:		Server timestamp, from the moment the handshake challenge was sent.
 *	- Cookie:			Cookie generated by the server, which the client must reply with.
 *	- AlignPad:			Handshake packets and PacketHandler's in general, require complex padding of packets. See ParseHandshakePacket.
 *
 *	- PacketSizeFiller:	Pads the client packet with blank information, so that the initial client packet,
 *						is the same size as the server response packet.
 *
 *						The server will ignore initial packets below/above this length. This prevents hijacking of game servers,
 *						for use in 'DRDoS' reflection amplification attacks.
 *
 *
 *
 * Game Protocol Changes:
 *
 * Every game (as opposed to handshake) packet starts with an extra bit, represented by [HandshakeBit], and game packets set this to 0.
 * This is the only change to game packets. When HandshakeBit is set to 1, the separate protocol above is used for handshake packets.
 *
 *
 *
 * HandshakeSecret/Cookie:
 *
 * The Cookie value is used to uniquely identify and perform a handshake with a connecting client,
 * but only the server can generate and recognize valid cookies, and the server must do this without storing any connection state data.
 *
 * To do this, the server stores 2 large random HandshakeSecret values, that only the server knows,
 * and combines that with data unique to the client connection (IP and Port), plus a server Timestamp, as part of generating the cookie.
 *
 * This data is then combined using a special HMAC hashing function, used specifically for authentication, to generate the cookie:
 *	Cookie = HMAC(HandshakeSecret, Timestamp + Client IP + Client Port)
 *
 * When the client responds to the handshake challenge, sending back TimeStamp and the Cookie,
 * the server will be able to collect all the remaining information it needs from the client packet (Client IP, Client Port),
 * plus the HandshakeSecret, to be able to regenerate the Cookie from scratch, and verify that the regenerated cookie,
 * is the same as the one the client sent back.
 *
 * No connection state data needs to be stored in order to do this, so this allows a stateless handshake.
 *
 *
 * In addition, HandshakeSecret updates every 15 + Rand(0,5) seconds (with previous value being stored/accepted for same amount of time)
 * in order to limit packet replay attacks, where a valid cookie can be reused multiple times.
 *
 * Checks on the handshake Timestamp, especially when combined with 5 second variance above, compliment this in limiting replay attacks.
 */



/**
 * Debug Defines
 */

// Enables packetloss testing, which should be tested by connecting/reconnecting to a server a couple dozen times.
// Every such connection attempt should eventually succeed/recover automatically - if any fail, something's broken.
#define PACKETLOSS_TEST 0


/**
 * Defines
 */

#define HANDSHAKE_PACKET_SIZE_BITS	194


// The number of seconds between secret value updates, and the random variance applied to this
#define SECRET_UPDATE_TIME			15.f
#define SECRET_UPDATE_TIME_VARIANCE	5.f

// The maximum allowed lifetime (in seconds) of any one handshake cookie
#define MAX_COOKIE_LIFETIME			((SECRET_UPDATE_TIME + SECRET_UPDATE_TIME_VARIANCE) * (float)SECRET_COUNT)

// The minimum amount of possible time a cookie may exist (for calculating when the clientside should timeout a challenge response)
#define MIN_COOKIE_LIFETIME			SECRET_UPDATE_TIME


/**
 * StatelessConnectHandlerComponent
 */

StatelessConnectHandlerComponent::StatelessConnectHandlerComponent()
	: HandlerComponent(FName(TEXT("StatelessConnectHandlerComponent")))
	, Driver(nullptr)
	, HandshakeSecret()
	, ActiveSecret(255)
	, LastSecretUpdateTimestamp(0.f)
	, LastChallengeSuccessAddress(TEXT(""))
	, LastServerSequence(0)
	, LastClientSequence(0)
	, LastClientSendTimestamp(0.0)
	, LastChallengeTimestamp(0.0)
	, LastSecretId(0)
	, LastTimestamp(0.f)
	, LastCookie()
{
	SetActive(true);

	bRequiresHandshake = true;
}

void StatelessConnectHandlerComponent::NotifyHandshakeBegin()
{
	if (Handler->Mode == Handler::Mode::Client)
	{
		UNetConnection* ServerConn = (Driver != nullptr ? Driver->ServerConnection : nullptr);

		if (ServerConn != nullptr)
		{
			FBitWriter InitialPacket(HANDSHAKE_PACKET_SIZE_BITS + 1 /* Termination bit */);
			uint8 bHandshakePacket = 1;

			InitialPacket.WriteBit(bHandshakePacket);


			// In order to prevent DRDoS reflection amplification attacks, clients must pad the packet to match server packet size
			uint8 SecretIdPad = 0;
			uint8 PacketSizeFiller[24];

			InitialPacket.WriteBit(SecretIdPad);

			FMemory::Memzero(PacketSizeFiller, ARRAY_COUNT(PacketSizeFiller));
			InitialPacket.Serialize(PacketSizeFiller, ARRAY_COUNT(PacketSizeFiller));



			CapHandshakePacket(InitialPacket);


			// Disable PacketHandler parsing, and send the raw packet
			Handler->SetRawSend(true);

#if !UE_BUILD_SHIPPING && PACKETLOSS_TEST
			bool bRandFail = FMath::RandBool();

			if (bRandFail)
			{
				UE_LOG(LogHandshake, Log, TEXT("Triggering random initial connect packet fail."));
			}

			if (!bRandFail)
#endif
			{
				if (ServerConn->Driver->IsNetResourceValid())
				{
					ServerConn->LowLevelSend(InitialPacket.GetData(), InitialPacket.GetNumBytes(), InitialPacket.GetNumBits());
				}
			}

			Handler->SetRawSend(false);

			LastClientSendTimestamp = FPlatformTime::Seconds();
		}
		else
		{
			UE_LOG(LogHandshake, Error, TEXT("Tried to send handshake connect packet without a server connection."));
		}
	}
}

void StatelessConnectHandlerComponent::SendConnectChallenge(FString ClientAddress)
{
	if (Driver != nullptr)
	{
		FBitWriter ChallengePacket(HANDSHAKE_PACKET_SIZE_BITS + 1 /* Termination bit */);
		uint8 bHandshakePacket = 1;
		float Timestamp = Driver->Time;
		uint8 Cookie[COOKIE_BYTE_SIZE];

		GenerateCookie(ClientAddress, ActiveSecret, Timestamp, Cookie);

		ChallengePacket.WriteBit(bHandshakePacket);
		ChallengePacket.WriteBit(ActiveSecret);

		ChallengePacket << Timestamp;
		ChallengePacket.Serialize(Cookie, ARRAY_COUNT(Cookie));

#if !UE_BUILD_SHIPPING
		UE_CLOG((Handler->DDoS == nullptr || !Handler->DDoS->CheckLogRestrictions()), LogHandshake, Log,
				TEXT("SendConnectChallenge. Timestamp: %f, Cookie: %s" ), Timestamp, *FString::FromBlob(Cookie, ARRAY_COUNT(Cookie)));
#endif

		CapHandshakePacket(ChallengePacket);

		
		// Disable PacketHandler parsing, and send the raw packet
		PacketHandler* ConnectionlessHandler = Driver->ConnectionlessHandler.Get();

		if (ConnectionlessHandler != nullptr)
		{
			ConnectionlessHandler->SetRawSend(true);
		}

#if !UE_BUILD_SHIPPING && PACKETLOSS_TEST
		bool bRandFail = FMath::RandBool();

		if (bRandFail)
		{
			UE_LOG(LogHandshake, Log, TEXT("Triggering random connect challenge packet fail."));
		}

		if (!bRandFail)
#endif
		{
			if (Driver->IsNetResourceValid())
			{
				Driver->LowLevelSend(ClientAddress, ChallengePacket.GetData(), ChallengePacket.GetNumBits());
			}
		}


		if (ConnectionlessHandler != nullptr)
		{
			ConnectionlessHandler->SetRawSend(false);
		}
	}
	else
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogHandshake, Error, TEXT("Tried to send handshake challenge packet without a net driver."));
#endif
	}
}

void StatelessConnectHandlerComponent::SendChallengeResponse(uint8 InSecretId, float InTimestamp, uint8 InCookie[COOKIE_BYTE_SIZE])
{
	UNetConnection* ServerConn = (Driver != nullptr ? Driver->ServerConnection : nullptr);

	if (ServerConn != nullptr)
	{
		FBitWriter ResponsePacket(HANDSHAKE_PACKET_SIZE_BITS + 1 /* Termination bit */);
		uint8 bHandshakePacket = 1;

		ResponsePacket.WriteBit(bHandshakePacket);
		ResponsePacket.WriteBit(InSecretId);

		ResponsePacket << InTimestamp;
		ResponsePacket.Serialize(InCookie, COOKIE_BYTE_SIZE);

#if !UE_BUILD_SHIPPING
		UE_LOG( LogHandshake, Log, TEXT( "SendChallengeResponse. Timestamp: %f, Cookie: %s" ), InTimestamp, *FString::FromBlob( InCookie, COOKIE_BYTE_SIZE ) );
#endif

		CapHandshakePacket(ResponsePacket);


		// Disable PacketHandler parsing, and send the raw packet
		Handler->SetRawSend(true);

#if !UE_BUILD_SHIPPING && PACKETLOSS_TEST
		bool bRandFail = FMath::RandBool();

		if (bRandFail)
		{
			UE_LOG(LogHandshake, Log, TEXT("Triggering random challenge response packet fail."));
		}

		if (!bRandFail)
#endif
		{
			if (ServerConn->Driver->IsNetResourceValid())
			{
				ServerConn->LowLevelSend(ResponsePacket.GetData(), ResponsePacket.GetNumBytes(), ResponsePacket.GetNumBits());
			}
		}

		Handler->SetRawSend(false);


		int16* CurSequence = (int16*)InCookie;

		LastClientSendTimestamp = FPlatformTime::Seconds();
		LastSecretId = InSecretId;
		LastTimestamp = InTimestamp;
		LastServerSequence = *CurSequence & (MAX_PACKETID - 1);
		LastClientSequence = *(CurSequence + 1) & (MAX_PACKETID - 1);

		FMemory::Memcpy(LastCookie, InCookie, ARRAY_COUNT(LastCookie));
	}
	else
	{
		UE_LOG(LogHandshake, Error, TEXT("Tried to send handshake response packet without a server connection."));
	}
}

void StatelessConnectHandlerComponent::SendChallengeAck(FString ClientAddress, uint8 InCookie[COOKIE_BYTE_SIZE])
{
	if (Driver != nullptr)
	{
		FBitWriter AckPacket(HANDSHAKE_PACKET_SIZE_BITS + 1 /* Termination bit */);
		uint8 bHandshakePacket = 1;
		float Timestamp  = -1.f;

		AckPacket.WriteBit(bHandshakePacket);
		AckPacket.WriteBit(bHandshakePacket);	// ActiveSecret

		AckPacket << Timestamp;
		AckPacket.Serialize(InCookie, COOKIE_BYTE_SIZE);

#if !UE_BUILD_SHIPPING
		UE_LOG(LogHandshake, Log, TEXT("SendChallengeAck. InCookie: %s" ), *FString::FromBlob(InCookie, COOKIE_BYTE_SIZE));
#endif

		CapHandshakePacket(AckPacket);

		
		// Disable PacketHandler parsing, and send the raw packet
		PacketHandler* ConnectionlessHandler = Driver->ConnectionlessHandler.Get();

		if (ConnectionlessHandler != nullptr)
		{
			ConnectionlessHandler->SetRawSend(true);
		}

#if !UE_BUILD_SHIPPING && PACKETLOSS_TEST
		bool bRandFail = FMath::RandBool();

		if (bRandFail)
		{
			UE_LOG(LogHandshake, Log, TEXT("Triggering random challenge ack packet fail."));
		}

		if (!bRandFail)
#endif
		{
			if (Driver->IsNetResourceValid())
			{
				Driver->LowLevelSend(ClientAddress, AckPacket.GetData(), AckPacket.GetNumBits());
			}
		}


		if (ConnectionlessHandler != nullptr)
		{
			ConnectionlessHandler->SetRawSend(false);
		}
	}
	else
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogHandshake, Error, TEXT("Tried to send handshake challenge ack packet without a net driver."));
#endif
	}
}

void StatelessConnectHandlerComponent::CapHandshakePacket(FBitWriter& HandshakePacket)
{
	check(HandshakePacket.GetNumBits() == HANDSHAKE_PACKET_SIZE_BITS);

	FPacketAudit::AddStage(TEXT("PostPacketHandler"), HandshakePacket);

	// Add a termination bit, the same as the UNetConnection code does
	HandshakePacket.WriteBit(1);
}

void StatelessConnectHandlerComponent::SetDriver(UNetDriver* InDriver)
{
	Driver = InDriver;

	if (Handler->Mode == Handler::Mode::Server)
	{
		StatelessConnectHandlerComponent* StatelessComponent = Driver->StatelessConnectComponent.Pin().Get();

		if (StatelessComponent != nullptr)
		{
			if (StatelessComponent == this)
			{
				UpdateSecret();
			}
			else
			{
				InitFromConnectionless(StatelessComponent);
			}
		}
	}
}

void StatelessConnectHandlerComponent::Initialize()
{
	// On the server, initializes immediately. Clientside doesn't initialize until handshake completes.
	if (Handler->Mode == Handler::Mode::Server)
	{
		Initialized();
	}
}

void StatelessConnectHandlerComponent::InitFromConnectionless(StatelessConnectHandlerComponent* InConnectionlessHandler)
{
	// Store the cookie/address used for the handshake, to enable server ack-retries
	LastChallengeSuccessAddress = InConnectionlessHandler->LastChallengeSuccessAddress;

	FMemory::Memcpy(LastCookie, InConnectionlessHandler->LastCookie, ARRAY_COUNT(LastCookie));
}

void StatelessConnectHandlerComponent::Incoming(FBitReader& Packet)
{
	bool bHandshakePacket = !!Packet.ReadBit() && !Packet.IsError();

	if (bHandshakePacket)
	{
		uint8 SecretId = 0;
		float Timestamp = 1.f;
		uint8 Cookie[COOKIE_BYTE_SIZE];

		bHandshakePacket = ParseHandshakePacket(Packet, SecretId, Timestamp, Cookie);

		if (bHandshakePacket)
		{
			if (Handler->Mode == Handler::Mode::Client)
			{
				if (State == Handler::Component::State::UnInitialized || State == Handler::Component::InitializedOnLocal)
				{
					// Receiving challenge, verify the timestamp is > 0.0f
					if (Timestamp > 0.0f)
					{
						LastChallengeTimestamp = (Driver != nullptr ? Driver->Time : 0.0);

						SendChallengeResponse(SecretId, Timestamp, Cookie);

						// Utilize this state as an intermediary, indicating that the challenge response has been sent
						SetState(Handler::Component::InitializedOnLocal);
					}
					// Receiving challenge ack, verify the timestamp is < 0.0f
					else if (Timestamp < 0.0f)
					{
						UNetConnection* ServerConn = (Driver != nullptr ? Driver->ServerConnection : nullptr);

						// Extract the initial packet sequence from the random Cookie data
						if (ensure(ServerConn != nullptr))
						{
							int16* CurSequence = (int16*)Cookie;

							int32 ServerSequence = *CurSequence & (MAX_PACKETID - 1);
							int32 ClientSequence = *(CurSequence + 1) & (MAX_PACKETID - 1);

							ServerConn->InitSequence(ServerSequence, ClientSequence);
						}

						// Now finish initializing the handler - flushing the queued packet buffer in the process.
						SetState(Handler::Component::Initialized);
						Initialized();
					}
				}
				else
				{
					// Ignore, could be a dupe/out-of-order challenge packet
				}
			}
			else if (Handler->Mode == Handler::Mode::Server)
			{
				// The server should not be receiving handshake packets at this stage - resend the ack in case it was lost.
				// In this codepath, this component is linked to a UNetConnection, and the Last* values below, cache the handshake info.
#if !UE_BUILD_SHIPPING
				UE_LOG(LogHandshake, Log, TEXT("Received unexpected post-connect handshake packet - resending ack." ));
#endif

				SendChallengeAck(LastChallengeSuccessAddress, LastCookie);
			}
		}
		else
		{
			Packet.SetError();

#if !UE_BUILD_SHIPPING
			UE_LOG(LogHandshake, Log, TEXT("Incoming: Error reading handshake packet."));
#endif
		}
	}
#if !UE_BUILD_SHIPPING
	else if (Packet.IsError())
	{
		UE_LOG(LogHandshake, Log, TEXT("Incoming: Error reading handshake bit from packet."));
	}
#endif
}

void StatelessConnectHandlerComponent::Outgoing(FBitWriter& Packet)
{
	// All UNetConnection packets must specify a zero bHandshakePacket value
	FBitWriter NewPacket(Packet.GetNumBits()+1);
	uint8 bHandshakePacket = 0;

	NewPacket.WriteBit(bHandshakePacket);
	NewPacket.SerializeBits(Packet.GetData(), Packet.GetNumBits());

	Packet.Reset();
	Packet.SerializeBits(NewPacket.GetData(), NewPacket.GetNumBits());
}

void StatelessConnectHandlerComponent::IncomingConnectionless(FString Address, FBitReader& Packet)
{
	bool bHandshakePacket = !!Packet.ReadBit() && !Packet.IsError();

	LastChallengeSuccessAddress.Empty();

	if (bHandshakePacket)
	{
		uint8 SecretId = 0;
		float Timestamp = 1.f;
		uint8 Cookie[COOKIE_BYTE_SIZE];

		bHandshakePacket = ParseHandshakePacket(Packet, SecretId, Timestamp, Cookie);

		if (bHandshakePacket)
		{
			if (Handler->Mode == Handler::Mode::Server)
			{
				bool bInitialConnect = Timestamp == 0.f;

				if (bInitialConnect)
				{
					SendConnectChallenge(Address);
				}
				// Challenge response
				else if (Driver != nullptr)
				{
					// NOTE: Allow CookieDelta to be 0.f, as it is possible for a server to send a challenge and receive a response,
					//			during the same tick
					bool bChallengeSuccess = false;
					float CookieDelta = Driver->Time - Timestamp;
					float SecretDelta = Timestamp - LastSecretUpdateTimestamp;
					bool bValidCookieLifetime = CookieDelta >= 0.0 && (MAX_COOKIE_LIFETIME - CookieDelta) > 0.f;
					bool bValidSecretIdTimestamp = (SecretId == ActiveSecret) ? (SecretDelta >= 0.f) : (SecretDelta <= 0.f);

					if (bValidCookieLifetime && bValidSecretIdTimestamp)
					{
						// Regenerate the cookie from the packet info, and see if the received cookie matches the regenerated one
						uint8 RegenCookie[COOKIE_BYTE_SIZE];

						GenerateCookie(Address, SecretId, Timestamp, RegenCookie);

						bChallengeSuccess = FMemory::Memcmp(Cookie, RegenCookie, COOKIE_BYTE_SIZE) == 0;

						if (bChallengeSuccess)
						{
							int16* CurSequence = (int16*)Cookie;

							LastServerSequence = *CurSequence & (MAX_PACKETID - 1);
							LastClientSequence = *(CurSequence + 1) & (MAX_PACKETID - 1);

							LastChallengeSuccessAddress = Address;


							// Now ack the challenge response, and store the cookie in LastCookie, to enable retries
							SendChallengeAck(Address, Cookie);

							FMemory::Memcpy(LastCookie, Cookie, ARRAY_COUNT(LastCookie));
						}
					}
				}
			}
		}
		else
		{
			Packet.SetError();

			if (Handler->DDoS != nullptr)
			{
				Handler->DDoS->IncBadPacketCounter();
			}

#if !UE_BUILD_SHIPPING
			UE_CLOG(Handler->DDoS == nullptr || !Handler->DDoS->CheckLogRestrictions(), LogHandshake, Log,
					TEXT("IncomingConnectionless: Error reading handshake packet."));
#endif
		}
	}
#if !UE_BUILD_SHIPPING
	else if (Packet.IsError())
	{
		UE_LOG(LogHandshake, Log, TEXT("IncomingConnectionless: Error reading handshake bit from packet."));
	}
#endif
}

bool StatelessConnectHandlerComponent::ParseHandshakePacket(FBitReader& Packet, uint8& OutSecretId, float& OutTimestamp,
															uint8 (&OutCookie)[COOKIE_BYTE_SIZE])
{
	bool bValidPacket = false;

	// Only accept handshake packets of precisely the right size
	if (Packet.GetBitsLeft() == (HANDSHAKE_PACKET_SIZE_BITS - 1))
	{
		OutSecretId = Packet.ReadBit();

		Packet << OutTimestamp;

		Packet.Serialize(OutCookie, COOKIE_BYTE_SIZE);

		bValidPacket = !Packet.IsError();
	}

	return bValidPacket;
}

void StatelessConnectHandlerComponent::GenerateCookie(FString ClientAddress, uint8 SecretId, float Timestamp, uint8 (&OutCookie)[20])
{
	// @todo #JohnB: Add cpu stats tracking, like what Oodle does upon compression
	//					NOTE: Being serverside, will only show up in .uprof, not on any 'stat' commands. Still necessary though.

	TArray<uint8> CookieData;
	FMemoryWriter CookieArc(CookieData);

	CookieArc << Timestamp;
	CookieArc << ClientAddress;

	FSHA1::HMACBuffer(HandshakeSecret[!!SecretId].GetData(), SECRET_BYTE_SIZE, CookieData.GetData(), CookieData.Num(), OutCookie);
}

void StatelessConnectHandlerComponent::UpdateSecret()
{
	LastSecretUpdateTimestamp = Driver != nullptr ? Driver->Time : 0.f;

	// On first update, update both secrets
	if (ActiveSecret == 255)
	{
		// NOTE: The size of this may be excessive.
		HandshakeSecret[0].AddUninitialized(SECRET_BYTE_SIZE);
		HandshakeSecret[1].AddUninitialized(SECRET_BYTE_SIZE);

		TArray<uint8>& CurArray = HandshakeSecret[1];

		for (int32 i=0; i<SECRET_BYTE_SIZE; i++)
		{
			CurArray[i] = FMath::Rand() % 255;
		}

		ActiveSecret = 0;
	}
	else
	{
		ActiveSecret = (uint8)!ActiveSecret;
	}

	TArray<uint8>& CurArray = HandshakeSecret[ActiveSecret];

	for (int32 i=0; i<SECRET_BYTE_SIZE; i++)
	{
		CurArray[i] = FMath::Rand() % 255;
	}
}

int32 StatelessConnectHandlerComponent::GetReservedPacketBits()
{
	int32 ReturnVal = 1;

#if !UE_BUILD_SHIPPING
	SET_DWORD_STAT(STAT_PacketReservedHandshake, ReturnVal);
#endif

	return ReturnVal;
}

void StatelessConnectHandlerComponent::Tick(float DeltaTime)
{
	if (Handler->Mode == Handler::Mode::Client)
	{
		if (State != Handler::Component::State::Initialized && LastClientSendTimestamp != 0.0)
		{
			double LastSendTimeDiff = FPlatformTime::Seconds() - LastClientSendTimestamp;

			if (LastSendTimeDiff > 1.0)
			{
				bool bRestartChallenge = Driver != nullptr && ((Driver->Time - LastChallengeTimestamp) > MIN_COOKIE_LIFETIME);

				if (bRestartChallenge)
				{
					SetState(Handler::Component::UnInitialized);
				}

				if (State == Handler::Component::State::UnInitialized)
				{
					UE_LOG(LogHandshake, Verbose, TEXT("Initial handshake packet timeout - resending."));

					NotifyHandshakeBegin();
				}
				else if (State == Handler::Component::InitializedOnLocal && LastTimestamp != 0.f)
				{
					UE_LOG(LogHandshake, Verbose, TEXT("Challenge response packet timeout - resending."));

					SendChallengeResponse(LastSecretId, LastTimestamp, LastCookie);
				}
			}
		}
	}
	else // if (Handler->Mode == Handler::Mode::Server)
	{
		bool bConnectionlessHandler = Driver != nullptr && Driver->StatelessConnectComponent.HasSameObject(this);

		if (bConnectionlessHandler)
		{
			static float CurVariance = FMath::FRandRange(0.f, SECRET_UPDATE_TIME_VARIANCE);

			// Update the secret value periodically, to reduce replay attacks. Also adds a bit of randomness to the timing of this,
			// so that handshake Timestamp checking as an added method of reducing replay attacks, is more effective.
			if (((Driver->Time - LastSecretUpdateTimestamp) - (SECRET_UPDATE_TIME + CurVariance)) > 0.f)
			{
				CurVariance = FMath::FRandRange(0.f, SECRET_UPDATE_TIME_VARIANCE);

				UpdateSecret();
			}
		}
	}
}

