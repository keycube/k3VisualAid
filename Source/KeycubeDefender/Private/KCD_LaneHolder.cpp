// Fill out your copyright notice in the Description page of Project Settings.


#include "KCD_LaneHolder.h"

// Sets default values
AKCD_LaneHolder::AKCD_LaneHolder()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SceneComponent = CreateDefaultSubobject<USceneComponent>("LaneRoot");
	RootComponent = SceneComponent;

	HitBox = CreateDefaultSubobject<UBoxComponent>("HitZone");
	HitBox->SetupAttachment(SceneComponent);

}

// Called when the game starts or when spawned
void AKCD_LaneHolder::BeginPlay()
{
	Super::BeginPlay();

	FillLanes();
	
}

// Called every frame
void AKCD_LaneHolder::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AKCD_LaneHolder::FillLanes()
{
	TArray<USceneComponent*> ChildComponents;
	SceneComponent->GetChildrenComponents(true, ChildComponents);

	for (auto ChildComponent : ChildComponents)
	{
		if(auto ChildActor = Cast<UChildActorComponent>(ChildComponent))
		if(auto LaneActor = Cast<AKCD_Lane>(ChildActor->GetChildActor()))
		{
			Lanes.Add(LaneActor);
		}
	}

}

