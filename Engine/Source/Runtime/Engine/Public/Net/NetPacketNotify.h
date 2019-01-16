// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EngineLogs.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogMacros.h"
#include "Templates/IsSigned.h"
#include "Util/ResizableCircularQueue.h"
#include "Util/SequenceNumber.h"
#include "Util/SequenceHistory.h"

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#	define UE_NET_ENABLE_PACKET_NOTIFY_LOG 0
#else
#	define UE_NET_ENABLE_PACKET_NOTIFY_LOG 1
#endif 

#if UE_NET_ENABLE_PACKET_NOTIFY_LOG
#	define UE_LOG_PACKET_NOTIFY(Format, ...)  UE_LOG(LogNetTraffic, Verbose, Format, ##__VA_ARGS__)
#else
#	define UE_LOG_PACKET_NOTIFY(...)
#endif

#define UE_LOG_PACKET_NOTIFY_WARNING(Format, ...)  UE_LOG(LogNetTraffic, Warning, Format, ##__VA_ARGS__)

struct FBitWriter;
struct FBitReader;

/** 
	FNetPacketNotify - Drives delivery of sequence numbers, acknowledgments and notifications of delivery sequence numbers
*/
class FNetPacketNotify
{
public:
	enum { SequenceNumberBits = 14 };
	enum { MaxSequenceHistoryLength = 256 };

	typedef TSequenceNumber<SequenceNumberBits, uint16> SequenceNumberT;
	typedef TSequenceHistory<MaxSequenceHistoryLength> SequenceHistoryT;

	struct FNotificationHeader
	{
		SequenceHistoryT History;
		SIZE_T HistoryWordCount;
		SequenceNumberT Seq;
		SequenceNumberT AckedSeq;
	};

	/** Constructor */
	FNetPacketNotify()
	: AckRecord(64)
	, WrittenHistoryWordCount(0)
	{}

	/** Init notification with expected initial sequence numbers */
	void Init(SequenceNumberT InitialInSeq, SequenceNumberT InitialOutSeq);

	/** Mark Seq as received and update current InSeq, missing sequence numbers will be marked as lost */
	void AckSeq(SequenceNumberT Seq) { AckSeq(Seq, true); }

	/** Explicitly mark Seq as not received and update current InSeq, additional missing sequence numbers will be marked as lost */
	void NakSeq(SequenceNumberT Seq) { AckSeq(Seq, false); }

	/** Increment outgoing seq number and commit data*/
	SequenceNumberT CommitAndIncrementOutSeq();

	/** Write NotificationHeader, and update outgoing ack record 
	 if bRefresh is true we will attempt to refresh a previously written header 
	if the size remains the same */
	void WriteHeader(FBitWriter& Writer, bool bRefresh = false);
	
	/** Read header from stream */
	bool ReadHeader(FNotificationHeader& Data, FBitReader& Reader) const;

	/** Update state of PacketNotification based on received header and invoke packet notifications for received acks.
		InFunc is a function in the format void)(FNetPacketNotify::SequenceNumberT AckedSequence, bool delivered) to handle packetNotifications.

		Returns the positive delta of the incoming seq number if it is within half the sequence number space
		Returns 0 if the received sequence number is out of the current window or if the acknowledged seq received by the remote is invalid.
	*/
	template<class Functor>
	SequenceNumberT::DifferenceT Update(const FNotificationHeader& NotificationData, Functor&& InFunc)
	{		
		if (NotificationData.Seq > InSeq && NotificationData.AckedSeq >= OutAckSeq)
		{
			UE_LOG_PACKET_NOTIFY(TEXT("FNetPacketNotify::Update - Seq %u, InSeq %u"), NotificationData.Seq.Get(), InSeq.Get());

			const SequenceNumberT::DifferenceT InSeqDelta = SequenceNumberT::Diff(NotificationData.Seq, InSeq);
			
			ProcessReceivedAcks(NotificationData, InFunc);

			// accept sequence
			InSeq = NotificationData.Seq;

			return InSeqDelta;
		}
		else
		{
			return 0;
		}
	}

	/** Get the current SequenceHistory */
	const SequenceHistoryT& GetInSeqHistory() const { return InSeqHistory; }

	/** Get the last received in sequence number */
	SequenceNumberT GetInSeq() const { return InSeq; }

	/** Get the last received sequence number that we have accepted, InAckSeq cannot be larger than InSeq */
	SequenceNumberT GetInAckSeq() const { return InAckSeq; }

	/** Get the current outgoing sequence number */
	SequenceNumberT GetOutSeq() const { return OutSeq; }

	/** Get the last outgoing sequence number acknowledged by remote */
	SequenceNumberT GetOutAckSeq() const { return OutAckSeq; }

	/** If we do have more unacknowledged sequence numbers in-flight than our maximum sendwindow we should not send more as the receiving end will not be able to detect if the sequence number has wrapped around */
	bool CanSend() const { SequenceNumberT NextOutSeq = OutSeq; ++NextOutSeq; return NextOutSeq >= OutAckSeq; }

	/** Get the current sequenceHistory length in bits */
	SIZE_T GetCurrentSequenceHistoryLength() const;

private:
	struct FSentAckData
	{
		SequenceNumberT OutSeq;	// Not needed... just to verify that things work as expected
		SequenceNumberT InAckSeq;
	};
	typedef TResizableCircularQueue<FSentAckData, TInlineAllocator<128>> AckRecordT;

	AckRecordT AckRecord;				// Track acked seq for each sent packet to track size of ack history
	SIZE_T WrittenHistoryWordCount;		// Bookkeeping to track if we can update data
	SequenceNumberT WrittenInAckSeq;	// When we call CommitAndIncrementOutSequence this will be committed along with the current outgoing sequence number for bookkeeping

	// Track incoming sequence data
	SequenceHistoryT InSeqHistory;		// BitBuffer containing a bitfield describing the history of received packets
	SequenceNumberT InSeq;				// Last sequence number received and accepted from remote
	SequenceNumberT InAckSeq;			// Last sequence number received from remote that we have acknowledged, this is needed since we support accepting a packet but explicitly not acknowledge it as received.
	SequenceNumberT InAckSeqAck;		// Last sequence number received from remote that we have acknowledged and also knows that the remote has received the ack, used to calculate how big our history must be

	// Track outgoing sequence data
	SequenceNumberT OutSeq;				// Outgoing sequence number
	SequenceNumberT OutAckSeq;			// Last sequence number that we know that the remote side have received.

private:
	SequenceNumberT UpdateInAckSeqAck(SequenceNumberT::DifferenceT AckCount, SequenceNumberT AckedSeq)
	{
		check((SIZE_T)AckCount <= AckRecord.Count());

		if (AckCount > 1)
		{
			AckRecord.PopNoCheck(AckCount - 1);
		}
		
		FSentAckData AckData = AckRecord.PeekNoCheck();
		AckRecord.PopNoCheck();

		// verify that we have a matching sequence number
		if (AckData.OutSeq == AckedSeq)
		{
			return AckData.InAckSeq;
		}
		else
		{
			UE_LOG_PACKET_NOTIFY_WARNING(TEXT("FNetPacketNotify::UpdateInAckSeqAck - Failed to find matching AckRecord for %u, (Found %u)"), AckedSeq.Get(), AckData.OutSeq.Get());

			// Pessimistic view, should never occur
			return SequenceNumberT(AckedSeq.Get() - MaxSequenceHistoryLength);
		}
	}

	template<class Functor>
	void ProcessReceivedAcks(const FNotificationHeader& NotificationData, Functor&& InFunc)
	{
		if (NotificationData.AckedSeq > OutAckSeq)
		{
			UE_LOG_PACKET_NOTIFY(TEXT("Notification::ProcessReceivedAcks - AckedSeq: %u, OutAckSeq: %u"), NotificationData.AckedSeq.Get(), OutAckSeq.Get());

			SequenceNumberT::DifferenceT AckCount = SequenceNumberT::Diff(NotificationData.AckedSeq, OutAckSeq);

			// Update InAckSeqAck used to track the needed number of bits to transmit our ack history
			InAckSeqAck = UpdateInAckSeqAck(AckCount, NotificationData.AckedSeq);

			// ExpectedAck = OutAckSeq + 1
			SequenceNumberT CurrentAck(OutAckSeq);
			++CurrentAck;

			// Everything not found in the history buffer is treated as lost
			while (AckCount > (SequenceNumberT::DifferenceT)(SequenceHistoryT::Size))
			{
				--AckCount;
				UE_LOG_PACKET_NOTIFY_WARNING(TEXT("Notification::ProcessReceivedAcks Seq: %u - IsAck: 0 HistoryIndex: N/A"), CurrentAck.Get());
				InFunc(CurrentAck, false);
				++CurrentAck;
			}

			// For sequence numbers contained in the history we lookup the delivery status from the history
			while (AckCount > 0)
			{
				--AckCount;
				UE_LOG_PACKET_NOTIFY(TEXT("Notification::ProcessReceivedAcks Seq: %u - IsAck: %u HistoryIndex: %u"), CurrentAck.Get(), NotificationData.History.IsDelivered(AckCount) ? 1u : 0u, AckCount);
				InFunc(CurrentAck, NotificationData.History.IsDelivered(AckCount));
				++CurrentAck;
			}
			OutAckSeq = NotificationData.AckedSeq;
		}
	}

	void AckSeq(SequenceNumberT AckedSeq, bool IsAck);

#if WITH_DEV_AUTOMATION_TESTS
	friend struct FNetPacketNotifyTestUtil;
#endif
};
