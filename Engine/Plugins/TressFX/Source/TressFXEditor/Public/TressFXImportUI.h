#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/ImportSettings.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"

struct TressFXImportOptions
{
public:
	FRotator PickedImportRotation = FRotator(0.0f, 0.0f, 0.0f);;
	FVector PickedImportTranslation = FVector(0.0f, 0.0f, 0.0f);
	FVector PickedScale = FVector(1.0f, 1.0f, 1.0f);
};

class STressFXImportWindow : public SWindow
{
public:
	SLATE_BEGIN_ARGS(STressFXImportWindow)
	{
	}
	SLATE_END_ARGS()

		STressFXImportWindow()
		: UserResponse(EAppReturnType::Cancel)
	{
	}

	void Construct(const FArguments& InArgs, bool AllowCancel = true, FString InTitle = "TressFX Import");

public:
	/** Displays the dialog in a blocking fashion */
	EAppReturnType::Type ShowModal();

	TressFXImportOptions GetImportOptions() { return ImportOptions; }
	

protected:

	FReply OnButtonClick(EAppReturnType::Type ButtonID);

	bool AllowCancelClick;
	EAppReturnType::Type UserResponse;
	   
	void SetRotation(float NewRot, EAxis::Type Axis);
	TOptional<float> GetRotation(EAxis::Type Axis) const;
	void SetTranslation(float NewTrans, EAxis::Type Axis);
	TOptional<float> GetTranslation(EAxis::Type Axis) const;
	void SetScale(float NewScale, EAxis::Type Axis);
	TOptional<float> GetScale(EAxis::Type Axis) const;
	
	TressFXImportOptions ImportOptions;
};