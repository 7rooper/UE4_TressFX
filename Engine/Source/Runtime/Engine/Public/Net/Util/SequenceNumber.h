// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "Templates/IsSigned.h"

/** Helper class to work with sequence numbers */
template <SIZE_T NumBits, typename SequenceType>
class TSequenceNumber
{
	static_assert(TIsSigned<SequenceType>::Value == false, "The base type for sequence numbers must be unsigned");

public:
	typedef SequenceType SequenceT;
	typedef int32 DifferenceT;

	// Constants
	enum { SeqNumberBits = NumBits };
	enum { SeqNumberCount = SequenceT(1) << NumBits };
	enum { SeqNumberHalf = SequenceT(1) << (NumBits - 1) };
	enum { SeqNumberMax = SeqNumberCount - 1u };
	enum { SeqNumberMask = SeqNumberMax };

	/** Default constructor */
	TSequenceNumber() : Value(0u) {}

	/** Constructor with given value */
	TSequenceNumber(SequenceT ValueIn) : Value(ValueIn & SeqNumberMask) {}
	
	/** Get Current Value */	
	SequenceT Get() const { return Value; }

	/** Diff between sequence numbers (A - B) only valid if (A - B) < SeqNumberHalf */
	static DifferenceT Diff(TSequenceNumber A, TSequenceNumber B);
	
	/** return true if this is > Other, this is only considered to be the case if (A - B) < SeqNumberHalf since we have to be able to detect wraparounds */
	bool operator>(const TSequenceNumber& Other) const { return (Value != Other.Value) && (((Value - Other.Value) & SeqNumberMask) < SeqNumberHalf); }

	/** Check if this is >= Other, See above */
	bool operator>=(const TSequenceNumber& Other) const { return ((Value - Other.Value) & SeqNumberMask) < SeqNumberHalf; }

	/** Equals, NOTE that sequence numbers wrap around so 0 == 0 + SequenceNumberCount */
	bool operator==(const TSequenceNumber& Other) const { return Value == Other.Value; }

	bool operator!=(const TSequenceNumber& Other) const { return Value != Other.Value; }

	/** Pre-increment and wrap around */
	TSequenceNumber& operator++() { Increment(1u); return *this; }
	
	/** Post-increment and wrap around */
	TSequenceNumber operator++(int) { TSequenceNumber Tmp(*this); Increment(1u); return Tmp; }

private:
	void Increment(SequenceT InValue) { *this = TSequenceNumber(Value + InValue); }
	SequenceT Value;
};

template <SIZE_T NumBits, typename SequenceType>
typename TSequenceNumber<NumBits, SequenceType>::DifferenceT TSequenceNumber<NumBits, SequenceType>::Diff(TSequenceNumber A, TSequenceNumber B) 
{ 
	const SIZE_T ShiftValue = sizeof(DifferenceT)*8 - NumBits;

	const SequenceT ValueA = A.Value;
	const SequenceT ValueB = B.Value;

	return (DifferenceT)((ValueA - ValueB) << ShiftValue) >> ShiftValue;
};
