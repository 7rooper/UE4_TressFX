/*
* Copyright (c) <2018> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
*/

#include "NiagaraDataInterfaceHoudiniCSV.h"
#include "NiagaraTypes.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE HOUDINI_NIAGARA_LOCTEXT_NAMESPACE 


UNiagaraDataInterfaceHoudiniCSV::UNiagaraDataInterfaceHoudiniCSV(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
    CSVFile = nullptr;
    //LastSpawnIndex = -1;
	LastSpawnedParticleID = -1;
}

void UNiagaraDataInterfaceHoudiniCSV::PostInitProperties()
{
    Super::PostInitProperties();

    if (HasAnyFlags(RF_ClassDefaultObject))
    {
	    FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);
    }

    GPUBufferDirty = true;
    //LastSpawnIndex = -1;
	LastSpawnedParticleID = -1;
}

void UNiagaraDataInterfaceHoudiniCSV::PostLoad()
{
    Super::PostLoad();
    GPUBufferDirty = true;
    //LastSpawnIndex = -1;
	LastSpawnedParticleID = -1;
}

#if WITH_EDITOR

void UNiagaraDataInterfaceHoudiniCSV::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceHoudiniCSV, CSVFile))
    {
		Modify();
		if (CSVFile)
		{
			GPUBufferDirty = true;
			//LastSpawnIndex = -1;
			LastSpawnedParticleID = -1;
		}
    }
}

#endif

bool UNiagaraDataInterfaceHoudiniCSV::CopyToInternal(UNiagaraDataInterface* Destination) const
{
    if ( !Super::CopyToInternal( Destination ) )
		return false;

    UNiagaraDataInterfaceHoudiniCSV* CastedInterface = CastChecked<UNiagaraDataInterfaceHoudiniCSV>( Destination );
    if ( !CastedInterface )
		return false;

    CastedInterface->CSVFile = CSVFile;

    return true;
}

bool UNiagaraDataInterfaceHoudiniCSV::Equals(const UNiagaraDataInterface* Other) const
{
    if ( !Super::Equals(Other) )
		return false;

    const UNiagaraDataInterfaceHoudiniCSV* OtherHNCSV = CastChecked<UNiagaraDataInterfaceHoudiniCSV>(Other);

    if ( OtherHNCSV != nullptr && OtherHNCSV->CSVFile != nullptr && CSVFile )
    {
		// Just make sure the two interfaces point to the same file
		return OtherHNCSV->CSVFile->FileName.Equals( CSVFile->FileName );
    }

    return false;
}


void UNiagaraDataInterfaceHoudiniCSV::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
    {
		// GetCSVFloatValue
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetCSVFloatValue");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));			// Col Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));	// Float Out

		OutFunctions.Add(Sig);
    }

	/*
    {
		// GetCSVFloatValueByString
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetCSVFloatValueByString");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));		// Row Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetStringDef(), TEXT("ColTitle")));	// Col Title In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));	// Float Out

		OutFunctions.Add(Sig);
    }
	*/

	{
		// GetCSVVectorValue
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetCSVVectorValue");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));			// Col Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));		// Vector3 Out

		OutFunctions.Add(Sig);
	}

    {
		// GetCSVPosition
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetCSVPosition");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));	// Vector3 Out

		OutFunctions.Add(Sig);
    }

    {
		// GetCSVPositionAndTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetCSVPositionAndTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));	// Vector3 Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		// float Out

		OutFunctions.Add(Sig);
    }

    {
		// GetCSVNormal
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetCSVNormal");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));	// Vector3 Out

		OutFunctions.Add(Sig);
    }

    {
		// GetCSVTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetCSVTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		// Float Out

		OutFunctions.Add(Sig);
    }

    {
		// GetNumberOfParticlesInCSV
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetNumberOfParticlesInCSV");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));					// CSV in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NumberOfParticles")));  // Int Out

		OutFunctions.Add(Sig);
    }

	{
		// GetNumberOfRowsInCSV
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetNumberOfRowsInCSV");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));					// CSV in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NumberOfRows")));		// Int Out

		OutFunctions.Add(Sig);
	}

    {
		// GetLastRowIndexAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetLastRowIndexAtTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));					// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));				// Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("LastRowIndex")));	    // Int Out

		OutFunctions.Add(Sig);
    }

    {
		// GetParticleIndexesToSpawnAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetParticleIndexesToSpawnAtTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("MinIndex")));	    // Int Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("MaxIndex")));	    // Int Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Count")));		    // Int Out

		OutFunctions.Add(Sig);
    }

	{
		// GetRowIndexesForParticleAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetRowIndexesForParticleAtTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));					// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("N")));					// Point Number In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));				// Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PreviousRow")));		// Int Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NextRow")));			// Int Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("PrevWeight")));		// Float Out

		OutFunctions.Add(Sig);
	}

	{
		// GetParticlePositionAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetParticlePositionAtTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("N")));				// Point Number In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));		// Vector3 Out

		OutFunctions.Add(Sig);
	}

	{
		// GetParticleValueAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetParticleValueAtTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("N")));				// Point Number In		
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));				// Col Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));		// Float Out

		OutFunctions.Add(Sig);
	}

	{
		// GetParticleVectorValueAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetParticleVectorValueAtTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("N")));				// Point Number In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));				// Col Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));			// Vector3 Out

		OutFunctions.Add(Sig);
	}

	{
		// GetParticleLifeAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetParticleLifeAtTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("N")));				// Point Number In		
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));		// Float Out

		OutFunctions.Add(Sig);
	}
}

// build the shader function HLSL; function name is passed in, as it's defined per-DI; that way, configuration could change
// the HLSL in the spirit of a static switch
// TODO: need a way to identify each specific function here
// 
bool UNiagaraDataInterfaceHoudiniCSV::GetFunctionHLSL(const FName& DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
    //FString BufferName = Descriptors[0].BufferParamName;
    //FString SecondBufferName = Descriptors[1].BufferParamName;

    /*
    if (InstanceFunctionName.Contains( TEXT( "GetCSVFloatValue") ) )
    {
	FString BufferName = Descriptors[0].BufferParamName;
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float In_Row, in float In_Col, out float Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value = ") + BufferName + TEXT("[(int)(In_Row + ( In_Col * ") + FString::FromInt(NumberOfRows) + TEXT(") ) ];");
	OutHLSL += TEXT("\n}\n");
    }
    else if (InstanceFunctionName.Contains(TEXT("GetCSVPosition")))
    {
	FString BufferName = Descriptors[1].BufferParamName;
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float In_N, out float3 Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value.x = ") + BufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\t Out_Value.y = ") + BufferName + TEXT("[(int)(In_N + ") + FString::FromInt(NumberOfRows) + TEXT(") ];");
	OutHLSL += TEXT("\t Out_Value.z = ") + BufferName + TEXT("[(int)(In_N + ( 2 * ") + FString::FromInt(NumberOfRows) + TEXT(") ) ];");
	OutHLSL += TEXT("\n}\n");
    }
    else if (InstanceFunctionName.Contains(TEXT("GetCSVNormal")))
    {
	FString BufferName = Descriptors[2].BufferParamName;
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float In_N, out float3 Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value.x = ") + BufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\t Out_Value.y = ") + BufferName + TEXT("[(int)(In_N + ") + FString::FromInt(NumberOfRows) + TEXT(") ];");
	OutHLSL += TEXT("\t Out_Value.z = ") + BufferName + TEXT("[(int)(In_N + ( 2 * ") + FString::FromInt(NumberOfRows) + TEXT(") ) ];");
	OutHLSL += TEXT("\n}\n");
    }
    else if (InstanceFunctionName.Contains(TEXT("GetCSVTime")))
    {
	FString BufferName = Descriptors[3].BufferParamName;
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float In_N, out float Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value = ") + BufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\n}\n");
    }
    else if (InstanceFunctionName.Contains( TEXT("GetNumberOfPointsInCSV") ) )
    {
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("( out int Out_Value ) \n{\n");
	OutHLSL += TEXT("\t Out_Value = ") + FString::FromInt(NumberOfRows) + TEXT(";");
	OutHLSL += TEXT("\n}\n");
    }*/
    /*else if (InstanceFunctionName.Contains(TEXT("GetCSVPositionAndTime")))
    {
	OutHLSL += TEXT("void ") + FunctionName + TEXT("(in float In_N, out float4 Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value.x = ") + BufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\t Out_Value.y = ") + BufferName + TEXT("[(int)(In_N + ") + FString::FromInt(NumberOfRows) + TEXT(") ];");
	OutHLSL += TEXT("\t Out_Value.z = ") + BufferName + TEXT("[(int)(In_N + ( 2 * ") + FString::FromInt(NumberOfRows) + TEXT(") ) ];");
	OutHLSL += TEXT("\t Out_Value.w = ") + SecondBufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\n}\n");
    }*/

    return !OutHLSL.IsEmpty();
}

// build buffer definition hlsl
// 1. Choose a buffer name, add the data interface ID (important!)
// 2. add a DIGPUBufferParamDescriptor to the array argument; that'll be passed on to the FNiagaraShader for binding to a shader param, that can
// then later be found by name via FindDIBufferParam for setting; 
// 3. store buffer declaration hlsl in OutHLSL
// multiple buffers can be defined at once here
//
void UNiagaraDataInterfaceHoudiniCSV::GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
    FString BufferName = "CSVData_" + ParamInfo.DataInterfaceHLSLSymbol;
    OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

    BufferName = "PositionData_" + ParamInfo.DataInterfaceHLSLSymbol;
    OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

    BufferName = "NormalData_" + ParamInfo.DataInterfaceHLSLSymbol;
    OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

    BufferName = "TimeData_" + ParamInfo.DataInterfaceHLSLSymbol;
    OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");
}

DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVFloatValue);
//DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVFloatValueByString);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVVectorValue);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVPosition);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVNormal);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVPositionAndTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetLastRowIndexAtTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetParticleIndexesToSpawnAtTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetRowIndexesForParticleAtTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetParticlePositionAtTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetParticleValueAtTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetParticleVectorValueAtTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetParticleLifeAtTime);
void UNiagaraDataInterfaceHoudiniCSV::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
    if (BindingInfo.Name == TEXT("GetCSVFloatValue") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
    {
		TNDIParamBinder<0, int32, TNDIParamBinder<1, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVFloatValue)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    /*else if (BindingInfo.Name == TEXT("GetCSVFloatValueByString") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
    {
		TNDIParamBinder<0, float, TNDIParamBinder<1, FString, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVFloatValueByString)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }*/
    else if (BindingInfo.Name == TEXT("GetCSVVectorValue") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
    {
		TNDIParamBinder<0, int32, TNDIParamBinder<1, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVVectorValue)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }    
    else if (BindingInfo.Name == TEXT("GetCSVPosition") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
		TNDIParamBinder<0, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVPosition)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetCSVNormal") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
		TNDIParamBinder<0, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVNormal)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetCSVTime") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
    {
		TNDIParamBinder<0, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVTime)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetCSVPositionAndTime") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 4)
    {
		TNDIParamBinder<0, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetCSVPositionAndTime)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if ( BindingInfo.Name == TEXT("GetNumberOfParticlesInCSV") && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 1 )
    {
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceHoudiniCSV::GetNumberOfParticlesInCSV);
    }
	else if (BindingInfo.Name == TEXT("GetNumberOfRowsInCSV") && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 1)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceHoudiniCSV::GetNumberOfRowsInCSV);
	}
    else if (BindingInfo.Name == TEXT("GetLastRowIndexAtTime") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
    {
		TNDIParamBinder<0, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetLastRowIndexAtTime)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetParticleIndexesToSpawnAtTime") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
		TNDIParamBinder<0, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetParticleIndexesToSpawnAtTime)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
	else if (BindingInfo.Name == TEXT("GetRowIndexesForParticleAtTime") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		TNDIParamBinder<0, int32, TNDIParamBinder<1, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetRowIndexesForParticleAtTime)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("GetParticlePositionAtTime") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		TNDIParamBinder<0, int32, TNDIParamBinder<1, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetParticlePositionAtTime)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("GetParticleValueAtTime") && BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 1)
	{
		TNDIParamBinder<0, int32, TNDIParamBinder<1, int32, TNDIParamBinder<2, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetParticleValueAtTime)>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("GetParticleVectorValueAtTime") && BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 3)
	{
		TNDIParamBinder<0, int32, TNDIParamBinder<1, int32, TNDIParamBinder<2, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetParticleVectorValueAtTime)>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("GetParticleLifeAtTime") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
	{
		TNDIParamBinder<0, int32, TNDIParamBinder<1, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetParticleLifeAtTime)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
    else
    {
		UE_LOG( LogHoudiniNiagara, Error, 
	    TEXT( "Could not find data interface function:\n\tName: %s\n\tInputs: %i\n\tOutputs: %i" ),
	    *BindingInfo.Name.ToString(), BindingInfo.GetNumInputs(), BindingInfo.GetNumOutputs() );
		OutFunc = FVMExternalFunction();
    }
}

template<typename RowParamType, typename ColParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetCSVFloatValue(FVectorVMContext& Context)
{
    RowParamType RowParam(Context);
    ColParamType ColParam(Context);

    FRegisterHandler<float> OutValue(Context);

    for ( int32 i = 0; i < Context.NumInstances; ++i )
    {
		int32 row = RowParam.Get();
		int32 col = ColParam.Get();
	
		float value = 0.0f;
		if ( CSVFile )
			CSVFile->GetCSVFloatValue( row, col, value );

		*OutValue.GetDest() = value;
		RowParam.Advance();
		ColParam.Advance();
		OutValue.Advance();
    }
}

template<typename RowParamType, typename ColParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetCSVVectorValue( FVectorVMContext& Context )
{
    RowParamType RowParam(Context);
    ColParamType ColParam(Context);

	FRegisterHandler<float> OutVectorX(Context);
	FRegisterHandler<float> OutVectorY(Context);
	FRegisterHandler<float> OutVectorZ(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 row = RowParam.Get();
		int32 col = ColParam.Get();

		FVector V = FVector::ZeroVector;
		if ( CSVFile )
			CSVFile->GetCSVVectorValue(row, col, V);

		*OutVectorX.GetDest() = V.X;
		*OutVectorY.GetDest() = V.Y;
		*OutVectorZ.GetDest() = V.Z;

		RowParam.Advance();
		ColParam.Advance();
		OutVectorX.Advance();
		OutVectorY.Advance();
		OutVectorZ.Advance();
    }
}
/*
template<typename RowParamType, typename ColTitleParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetCSVFloatValueByString(FVectorVMContext& Context)
{
    RowParamType RowParam(Context);
    ColTitleParamType ColTitleParam(Context);

    FRegisterHandler<float> OutValue(Context);

    for ( int32 i = 0; i < Context.NumInstances; ++i )
    {
		int32 row = RowParam.Get();
		FString colTitle = ColTitleParam.Get();
	
		float value = 0.0f;
		if ( CSVFile )
			CSVFile->GetCSVFloatValue( row, colTitle, value );

		*OutValue.GetDest() = value;
		RowParam.Advance();
		ColTitleParam.Advance();
		OutValue.Advance();
    }
}
*/

template<typename RowParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetCSVPosition(FVectorVMContext& Context)
{
	RowParamType RowParam(Context);
    FRegisterHandler<float> OutSampleX(Context);
    FRegisterHandler<float> OutSampleY(Context);
    FRegisterHandler<float> OutSampleZ(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 row = RowParam.Get();

		FVector V = FVector::ZeroVector;
		if ( CSVFile )
			CSVFile->GetCSVPositionValue( row, V );

		*OutSampleX.GetDest() = V.X;
		*OutSampleY.GetDest() = V.Y;
		*OutSampleZ.GetDest() = V.Z;
		RowParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
    }
}

template<typename RowParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetCSVNormal(FVectorVMContext& Context)
{
	RowParamType RowParam(Context);
    FRegisterHandler<float> OutSampleX(Context);
    FRegisterHandler<float> OutSampleY(Context);
    FRegisterHandler<float> OutSampleZ(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 row = RowParam.Get();

		FVector V = FVector::ZeroVector;
		if ( CSVFile )
			CSVFile->GetCSVNormalValue( row, V );

		*OutSampleX.GetDest() = V.X;
		*OutSampleY.GetDest() = V.Y;
		*OutSampleZ.GetDest() = V.Z;
		RowParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
    }
}

template<typename RowParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetCSVTime(FVectorVMContext& Context)
{
	RowParamType RowParam(Context);
    FRegisterHandler<float> OutValue(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 row = RowParam.Get();

		float value = 0.0f;
		if ( CSVFile )
			CSVFile->GetCSVTimeValue( row, value );

		*OutValue.GetDest() = value;
		RowParam.Advance();
		OutValue.Advance();
    }
}

// Returns the last index of the particles that should be spawned at time t
template<typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetLastRowIndexAtTime(FVectorVMContext& Context)
{
    TimeParamType TimeParam(Context);
    FRegisterHandler<int32> OutValue(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		float t = TimeParam.Get();

		int32 value = 0;
		if ( CSVFile )
			CSVFile->GetLastRowIndexAtTime( t, value );

		*OutValue.GetDest() = value;
		TimeParam.Advance();
		OutValue.Advance();
    }
}

// Returns the last index of the particles that should be spawned at time t
template<typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetParticleIndexesToSpawnAtTime( FVectorVMContext& Context )
{
    TimeParamType TimeParam( Context );
    FRegisterHandler<int32> OutMinValue( Context );
    FRegisterHandler<int32> OutMaxValue( Context );
    FRegisterHandler<int32> OutCountValue( Context );

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		float t = TimeParam.Get();

		int32 value = 0;
		int32 min = 0, max = 0, count = 0;
		if ( CSVFile )
		{
			if ( !CSVFile->GetLastParticleIndexAtTime( t, value ) )
			{
				// The CSV file doesn't have time informations, so always return all points in the file
				min = 0;
				max = value;
				count = max - min + 1;
			}
			else
			{
				// The CSV file has time informations
				// First, detect if we need to reset LastSpawnIndex (after a loop)
				if ( value < LastSpawnedParticleID )
					LastSpawnedParticleID = -1;

				if ( value < 0 )
				{
					// Nothing to spawn, t is lower than the particle's time
					LastSpawnedParticleID = -1;
				}
				else
				{
					// The last time value in the CSV is lower than t, spawn everything if we didnt already!
					if ( value >= CSVFile->GetNumberOfParticlesInCSV() )
						value = value - 1;

					if ( value == LastSpawnedParticleID)
					{
						// We dont have any new particle to spawn
						min = value;
						max = value;
						count = 0;
					}
					else
					{
						// We have particles to spawn at time t
						min = LastSpawnedParticleID + 1;
						max = value;
						count = max - min + 1;

						LastSpawnedParticleID = max;
					}
				}
			}
		}

		*OutMinValue.GetDest() = min;
		*OutMaxValue.GetDest() = max;
		*OutCountValue.GetDest() = count;

		TimeParam.Advance();
		OutMinValue.Advance();
		OutMaxValue.Advance();
		OutCountValue.Advance();
    }
}

template<typename RowParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetCSVPositionAndTime(FVectorVMContext& Context)
{
    RowParamType RowParam(Context);

    FRegisterHandler<float> OutPosX(Context);
    FRegisterHandler<float> OutPosY(Context);
    FRegisterHandler<float> OutPosZ(Context);
    FRegisterHandler<float> OutTime(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 row = RowParam.Get();

		float timeValue = 0.0f;
		FVector posVector = FVector::ZeroVector;
		if ( CSVFile )
		{
			CSVFile->GetCSVTimeValue( row, timeValue);
			CSVFile->GetCSVPositionValue( row, posVector);
		}

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		*OutTime.GetDest() = timeValue;

		RowParam.Advance();
		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
		OutTime.Advance();
    }
}

template<typename NParamType, typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetRowIndexesForParticleAtTime(FVectorVMContext& Context)
{
	NParamType NParam(Context);
	TimeParamType TimeParam(Context);

	FRegisterHandler<int32> OutPrevIndex(Context);
	FRegisterHandler<int32> OutNextIndex(Context);
	FRegisterHandler<float> OutWeightValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 N = NParam.Get();
		float time = TimeParam.Get();

		float weight = 0.0f;
		int32 prevIdx = 0;
		int32 nextIdx = 0;
		if ( CSVFile )
		{
			CSVFile->GetParticleLineIndexAtTime( N, time, prevIdx, nextIdx, weight );
		}

		*OutPrevIndex.GetDest() = prevIdx;
		*OutNextIndex.GetDest() = nextIdx;
		*OutWeightValue.GetDest() = weight;

		NParam.Advance();
		TimeParam.Advance();
		OutPrevIndex.Advance();
		OutNextIndex.Advance();
		OutWeightValue.Advance();
    }
}

template<typename NParamType, typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetParticlePositionAtTime( FVectorVMContext& Context )
{
	NParamType NParam(Context);
	TimeParamType TimeParam(Context);

	FRegisterHandler<float> OutPosX(Context);
	FRegisterHandler<float> OutPosY(Context);
	FRegisterHandler<float> OutPosZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 N = NParam.Get();
		float time = TimeParam.Get();

		FVector posVector = FVector::ZeroVector;
		if ( CSVFile )
		{
			CSVFile->GetParticlePositionAtTime(N, time, posVector);
		}		

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		NParam.Advance();
		TimeParam.Advance();
		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
    }
}

template<typename NParamType, typename ColParamType, typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetParticleValueAtTime(FVectorVMContext& Context)
{
	NParamType NParam(Context);
	TimeParamType TimeParam(Context);
	ColParamType ColParam(Context);

	FRegisterHandler<float> OutValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 N = NParam.Get();
		int32 Col = ColParam.Get();
		float time = TimeParam.Get();		

		float Value = 0.0f;
		if ( CSVFile )
		{
			CSVFile->GetParticleValueAtTime( N, Col, time, Value );
		}

		*OutValue.GetDest() = Value;

		NParam.Advance();
		ColParam.Advance();
		TimeParam.Advance();

		OutValue.Advance();
	}
}

template<typename NParamType, typename ColParamType, typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetParticleVectorValueAtTime(FVectorVMContext& Context)
{
	NParamType NParam(Context);
	ColParamType ColParam(Context);
	TimeParamType TimeParam(Context);	

	FRegisterHandler<float> OutPosX(Context);
	FRegisterHandler<float> OutPosY(Context);
	FRegisterHandler<float> OutPosZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 N = NParam.Get();
		int32 Col = ColParam.Get();
		float time = TimeParam.Get();		

		FVector posVector = FVector::ZeroVector;
		if ( CSVFile )
		{
			CSVFile->GetParticleVectorValueAtTime(N, Col, time, posVector, true, true);
		}

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		NParam.Advance();
		ColParam.Advance();
		TimeParam.Advance();
		
		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
	}
}

template<typename NParamType, typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetParticleLifeAtTime(FVectorVMContext& Context)
{
	NParamType NParam(Context);
	TimeParamType TimeParam(Context);

	FRegisterHandler<float> OutValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 N = NParam.Get();
		float time = TimeParam.Get();

		float Value = 0.0f;
		if ( CSVFile )
		{
			CSVFile->GetParticleLifeAtTime(N, time, Value);
		}

		*OutValue.GetDest() = Value;

		NParam.Advance();
		TimeParam.Advance();

		OutValue.Advance();
	}
}

void UNiagaraDataInterfaceHoudiniCSV::GetNumberOfRowsInCSV(FVectorVMContext& Context)
{
    FRegisterHandler<int32> OutNumRows(Context);
    *OutNumRows.GetDest() = CSVFile ? CSVFile->GetNumberOfLinesInCSV() : 0;
	OutNumRows.Advance();
}

void UNiagaraDataInterfaceHoudiniCSV::GetNumberOfParticlesInCSV(FVectorVMContext& Context)
{
	FRegisterHandler<int32> OutNumParticles(Context);
	*OutNumParticles.GetDest() = CSVFile ? CSVFile->GetNumberOfParticlesInCSV() : 0;
	OutNumParticles.Advance();
}

#undef LOCTEXT_NAMESPACE
