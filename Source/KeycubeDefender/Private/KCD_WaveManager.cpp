// Fill out your copyright notice in the Description page of Project Settings.


#include "KCD_WaveManager.h"

#include "KCD_Cube.h"
#include "KCD_LaneHolder.h"
#include "KCD_WordDictionnary.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AKCD_WaveManager::AKCD_WaveManager()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	WordIndexUsed.Add(FEncapsule{});
	WordIndexUsed.Add(FEncapsule{});
	WordIndexUsed.Add(FEncapsule{});
}

// Called when the game starts or when spawned
void AKCD_WaveManager::BeginPlay()
{
	Super::BeginPlay();
	
	//Get the current game mode and cast it to the required game mode
	GameModeInstance = Cast<AKCD_GameMode>(UGameplayStatics::GetGameMode(this));

	FTimerHandle TimerHandleGamemodeRefs;

	// Fetch the references set in the gamemode after a delay
	//to allow it to finish fetching them
	GetWorld()->GetTimerManager().SetTimer(TimerHandleGamemodeRefs, [&]()
	{
		LaneHolder = GameModeInstance->GetLaneHolder();
		if(LaneHolder == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("LaneHolder is invalid"));
		} else
		{
			LaneHolder->OnShipCrashedDelegate.AddDynamic(this, &AKCD_WaveManager::GameFinished);
		}
	},  0.1, false);

	NextWave();
}

void AKCD_WaveManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	UWorld* w= GetWorld() ;

	if(w->IsValidLowLevel())
	{
		w->GetTimerManager().ClearAllTimersForObject(this);
	}
}

void AKCD_WaveManager::PrepareShip(int ShipTier)
{
	//Get all the possible words for the ship's tier
	FString ShipTierString = FString::FromInt(ShipTier);
	FKCD_WordDictionnary* possibleWords = WordBank->FindRow<FKCD_WordDictionnary>(FName(ShipTierString), "");

	bool wordFound = false;

	//Get a random word in the list of possible ones
	//We loop so we can find an available word if the one chosen isn't
	int ShipWordIndex = FMath::RandRange(0, possibleWords[0].WordList.Num() - 1);;
	for (int x = 0; x <= possibleWords->WordList.Num(); x++)
	{
		
		if (WordIndexUsed[ShipTier].index.Contains(ShipWordIndex))
		{
			ShipWordIndex++;
			if(ShipWordIndex >= possibleWords->WordList.Num())
				ShipWordIndex = 0;
			continue;
		}

		wordFound = true;
		WordIndexUsed[ShipTier].index.Add(ShipWordIndex);
		break;
	}

	if (!wordFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("Word not found"));
		return;
	}

	//Use a deferred spawn so we can set the ship's word before spawning it
	AKCD_Ship* Ship;

	auto lane = FetchRandomLane();
	if (lane == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Couldn't get a valid lane"));
		return;
	}

	Ship = lane->SpawnShip(Ships[ShipTier], ShipWordIndex, possibleWords[0].WordList[ShipWordIndex], CurrentWaveData.SpeedModifier);

	ShipsAlive.Add(Ship);

	OnShipSpawnDelegate.Broadcast();

	Ship->OnShipDestroyedDelegate.AddDynamic(this, &AKCD_WaveManager::RemoveShip);
}

void AKCD_WaveManager::NextWave()
{
	CurrentWaveIndex++;
	
	GenerateWaveData(CurrentWaveIndex);

	//Start a looping timer to spawn ships at an interval
	GetWorld()->GetTimerManager().SetTimer(SpawnTimerHandle, this, &AKCD_WaveManager::PlayWaveSequence,
		CurrentWaveData.SpawnTime, true);
}

void AKCD_WaveManager::PlayWaveSequence()
{
	int ShipTier = CurrentWaveData.availableTiers[FMath::RandRange(0, CurrentWaveData.availableTiers.Num() - 1)];

	PrepareShip(ShipTier);

	--CurrentWaveData.NumShipTier[ShipTier];
	//Once all the ships for a tier are spawned,
	//remove the tier from the list of possible tiers
	if (CurrentWaveData.NumShipTier[ShipTier] <= 0)
		CurrentWaveData.availableTiers.Remove(ShipTier);

	//Stop the spawning loop when all ships are spawned
	if(CurrentWaveData.availableTiers.IsEmpty())
	{
		GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
		CurrentWaveData.NumShipTier.Empty();
	}
}

void AKCD_WaveManager::GenerateWaveData(int waveIndex)
{
	//Calculate a rounded down number of each ship tiers
	CurrentWaveData.NumShipTier.Add(0, floor(LikelihoodValues[0] * waveIndex));
	if(CurrentWaveData.NumShipTier[0] > 0)
		CurrentWaveData.availableTiers.Add(0);
	
	CurrentWaveData.NumShipTier.Add(1, floor(LikelihoodValues[1] * waveIndex));
	if(CurrentWaveData.NumShipTier[1] > 0)
		CurrentWaveData.availableTiers.Add(1);
	
	CurrentWaveData.NumShipTier.Add(2, floor(LikelihoodValues[2] * waveIndex));
	if(CurrentWaveData.NumShipTier[2] > 0)
		CurrentWaveData.availableTiers.Add(2);

	//Calculate speed and spawn time of this wave
	CurrentWaveData.SpawnTime = BaseSpawnTime / ( 1 + (waveIndex * SpawnDampener));
	CurrentWaveData.SpeedModifier = BaseSpeed * ( 1 + (waveIndex / SpeedDampener));
}

void AKCD_WaveManager::RemoveShip(AKCD_Ship* Ship)
{
	ShipsAlive.Remove(Ship);
	Ship->OnShipDestroyedDelegate.RemoveAll(this);
	WordIndexUsed[Ship->Tier].index.Remove(Ship->WordIndex);

	//When it is the last ship of the wave, we wait a bit then start the new wave
	if(ShipsAlive.IsEmpty() && CurrentWaveData.availableTiers.IsEmpty())
	{
		OnWaveCompleteDelegate.Broadcast(CurrentWaveIndex);
		
		GetWorld()->GetTimerManager().SetTimer(NewWaveTimerHandle, this, &AKCD_WaveManager::NextWave,
			3, false);
	}
}

AKCD_Lane* AKCD_WaveManager::FetchRandomLane()
{
	return LaneHolder->Lanes[FMath::RandRange(0, LaneHolder->Lanes.Num() - 1)];
}

void AKCD_WaveManager::GameFinished()
{
	AverageStats();
}

void AKCD_WaveManager::AverageStats()
{
	FKCD_TypingStats MainAverageStat;
	FKCD_TypingStats AltAverageStat;

	//Makes an average of all main target stats
	for (auto TypeStat : MainTypingStats)
	{
		MainAverageStat.Mistakes += TypeStat.Mistakes;
		MainAverageStat.Score += TypeStat.Score;
		MainAverageStat.TimeTaken += TypeStat.TimeTaken;
		MainAverageStat.WordDistance += TypeStat.WordDistance;
		MainAverageStat.WordSize += TypeStat.WordSize;
	}

	int size = MainTypingStats.Num();
	
	MainAverageStat.Mistakes = MainAverageStat.Mistakes / size;
	MainAverageStat.Score = MainAverageStat.Score / size;
	MainAverageStat.TimeTaken = MainAverageStat.TimeTaken / size;
	MainAverageStat.WordDistance = MainAverageStat.WordDistance / size;
	MainAverageStat.WordSize = MainAverageStat.WordSize / size;
	MainAverageStat.WasAltTarget = false;

	//We don't write any data for the alt target if there are none to write
	if(AltTypingStats.IsEmpty())
	{
		WriteStats(MainAverageStat);
		return;
	}
	
	//Makes an average of all alt target stats
	for (auto TypeStat : AltTypingStats)
	{
		AltAverageStat.Mistakes += TypeStat.Mistakes;
		AltAverageStat.Score += TypeStat.Score;
		AltAverageStat.TimeTaken += TypeStat.TimeTaken;
		AltAverageStat.WordDistance += TypeStat.WordDistance;
		AltAverageStat.WordSize += TypeStat.WordSize;
	}

	size = AltTypingStats.Num();
	
	AltAverageStat.Mistakes = AltAverageStat.Mistakes / size;
	AltAverageStat.Score = AltAverageStat.Score / size;
	AltAverageStat.TimeTaken = AltAverageStat.TimeTaken / size;
	AltAverageStat.WordDistance = AltAverageStat.WordDistance / size;
	AltAverageStat.WordSize = AltAverageStat.WordSize / size;
	AltAverageStat.WasAltTarget = false;

	WriteStats(MainAverageStat, AltAverageStat);
}

void AKCD_WaveManager::WriteStats(FKCD_TypingStats MainStat)
{
	//Set the relative path where the file is saved and the name of the file
	FString RelativePath = FPaths::ProjectContentDir();
	std::string path = (std::string((TCHAR_TO_UTF8(*RelativePath))
		+ std::string("ShipStats.csv")));

	//Open the file in append mode and check if it is opened
	std::ofstream myfile (path, std::ios::app);
	if (myfile.is_open())
	{
		//Create an FString with all main results
		FString MainResultFString;
		MainResultFString = "," + FString::FromInt(CurrentWaveIndex) + "," + "Main Targets," + FString::SanitizeFloat(MainStat.Score) + "," +
			FString::SanitizeFloat(MainStat.TimeTaken) + "," +
				FString::SanitizeFloat(MainStat.Mistakes) + "," +
					FString::SanitizeFloat(MainStat.WordSize) + "," +
						FString::SanitizeFloat(MainStat.WordDistance) + ",";

		//Convert the FString into a std::string
		std::string MainResultString = std::string(TCHAR_TO_UTF8(*MainResultFString));

		myfile << MainResultString + "\n";

		//Close the file
		myfile.close();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to open file for write"));
	}
}

void AKCD_WaveManager::WriteStats(FKCD_TypingStats MainStat, FKCD_TypingStats AltStat)
{
	//Set the relative path where the file is saved and the name of the file
	FString RelativePath = FPaths::ProjectContentDir();
	std::string path = (std::string((TCHAR_TO_UTF8(*RelativePath))
		+ std::string("ShipStats.csv")));

	//Open the file in append mode and check if it is opened
	std::ofstream myfile (path, std::ios::app);
	if (myfile.is_open())
	{
		//Create an FString with all main results
		FString MainResultFString;
		MainResultFString = "," + FString::FromInt(CurrentWaveIndex) + "," + "Main Targets," + FString::SanitizeFloat(MainStat.Score) + "," +
			FString::SanitizeFloat(MainStat.TimeTaken) + "," +
				FString::SanitizeFloat(MainStat.Mistakes) + "," +
					FString::SanitizeFloat(MainStat.WordSize) + "," +
						FString::SanitizeFloat(MainStat.WordDistance) + ",";

		//Convert the FString into a std::string
		std::string MainResultString = std::string(TCHAR_TO_UTF8(*MainResultFString));
		//Data
		myfile << MainResultString + "\n";

		//Create an FString with all alt results
		FString AltResultFString;
		AltResultFString = ",,Alt Targets," + FString::SanitizeFloat(AltStat.Score) + "," +
			FString::SanitizeFloat(AltStat.TimeTaken) + "," +
			FString::SanitizeFloat(AltStat.Mistakes) + "," +
			FString::SanitizeFloat(AltStat.WordSize) + "," +
			FString::SanitizeFloat(AltStat.WordDistance) + ",";

		//Convert the FString into a std::string
		std::string AltResultString = std::string(TCHAR_TO_UTF8(*AltResultFString));
		//Data
		myfile << AltResultString + "\n";
		
		//Close the file
		myfile.close();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to open file for write"));
	}
}

TArray<AKCD_Ship*> AKCD_WaveManager::GetValidShips(FName Letter)
{
	//Verify if there is a ship alive
	if (ShipsAlive.IsEmpty())
		return TArray<AKCD_Ship*>();

	TArray<AKCD_Ship*> matchingShips;

	//We go trough the list of ship to find one who's first
	//letter to destroy is the one we try to shoot
	for (AKCD_Ship* shipChecked : ShipsAlive)
	{
		if (shipChecked->isDestroyed)
			continue;
		if (shipChecked->LettersInstances[0]->CurrentLetter == Letter)
		{
			matchingShips.Add(shipChecked);
		}
	}
	
	return matchingShips;
}

void AKCD_WaveManager::AddStats(FKCD_TypingStats StatsToAdd)
{
	if(!StatsToAdd.WasAltTarget)
		MainTypingStats.Add(StatsToAdd);
	else
		AltTypingStats.Add(StatsToAdd);
}
