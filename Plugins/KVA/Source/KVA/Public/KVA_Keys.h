﻿#pragma once
#include "Components/TextRenderComponent.h"
#include "KVA_Keys.generated.h"

UCLASS()
class KVA_API AKVA_Keys : public AActor
{
	GENERATED_BODY()
	
	//component of the key object
	UPROPERTY(VisibleAnywhere, Category=Mesh)
	UStaticMeshComponent* KeyMesh;
	UPROPERTY(VisibleAnywhere, Category=Text)
	UTextRenderComponent* KeyText;
	
public:

	AKVA_Keys();

	//Materials and color for the feedback
	UPROPERTY(EditAnywhere, Category = Variables)
	FLinearColor PressedColor = FLinearColor::Green;
	UPROPERTY(EditAnywhere, Category = Variables)
	FLinearColor BaseColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, Category = Variables)
	FLinearColor HighlighColor = FLinearColor::Blue;
	UPROPERTY(EditAnywhere, Category = Variables)
	UMaterialInterface* BaseMaterial;

	//Input linked to the key
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Variables)
	FKey AssociatedKey;
	//highlight state handle
	UPROPERTY(EditAnywhere, Category = Variables)
	bool IsHighlighted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite ,Category = Variables)
	UMaterialInstanceDynamic* DynamicMat;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;


private:
	
	UPROPERTY()
	bool IsPressed = false;

public:
	
	//Visual feedback of the key press
	UFUNCTION(BlueprintCallable, Category = Feedback)
	void KeyPressed_Keys();
	//Visual feedback of the key release
	UFUNCTION(BlueprintCallable, Category = Feedback)
	void KeyReleased_Keys();
	//Visual feedback of the key highlight
	UFUNCTION(BlueprintCallable, Category = Feedback)
	void HighlightKey();
	//Remove visual feedback of the key highlight
	UFUNCTION(BlueprintCallable, Category = Feedback)
	void UnhighlightKey();
	//Change the associated key
	UFUNCTION(BlueprintCallable, Category = Feedback)
	void ChangeKey(FKey NewKey);
};
