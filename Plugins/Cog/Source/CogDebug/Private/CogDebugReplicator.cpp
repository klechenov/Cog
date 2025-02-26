#include "CogDebugReplicator.h"

#include "CogDebugDraw.h"
#include "CogDebugLog.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

//--------------------------------------------------------------------------------------------------------------------------
// ACogDebugReplicator
//--------------------------------------------------------------------------------------------------------------------------
ACogDebugReplicator* ACogDebugReplicator::Spawn(APlayerController* Controller)
{
    if (Controller->GetWorld()->GetNetMode() == NM_Client)
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnInfo;
    SpawnInfo.Owner = Controller;
    return Controller->GetWorld()->SpawnActor<ACogDebugReplicator>(SpawnInfo);
}

//--------------------------------------------------------------------------------------------------------------------------
ACogDebugReplicator* ACogDebugReplicator::GetLocalReplicator(UWorld& World)
{
    for (TActorIterator<ACogDebugReplicator> It(&World, ACogDebugReplicator::StaticClass()); It; ++It)
    {
        ACogDebugReplicator* Replicator = *It;
        return Replicator;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------------------------------
void ACogDebugReplicator::GetRemoteReplicators(UWorld& World, TArray<ACogDebugReplicator*>& Replicators)
{
    for (TActorIterator<ACogDebugReplicator> It(&World, ACogDebugReplicator::StaticClass()); It; ++It)
    {
        ACogDebugReplicator* Replicator = Cast<ACogDebugReplicator>(*It);
        Replicators.Add(Replicator);
    }
}

//--------------------------------------------------------------------------------------------------------------------------
ACogDebugReplicator::ACogDebugReplicator(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
#if !UE_BUILD_SHIPPING

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bAllowTickOnDedicatedServer = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    bReplicates = true;
    bOnlyRelevantToOwner = true;

    bHasAuthority = false;
    ReplicatedData.Owner = this;

#endif // !UE_BUILD_SHIPPING
}

//--------------------------------------------------------------------------------------------------------------------------
void ACogDebugReplicator::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    check(World);
    const ENetMode NetMode = World->GetNetMode();
    bHasAuthority = NetMode != NM_Client;

    OwnerPlayerController = Cast<APlayerController>(GetOwner());

    if (OwnerPlayerController->IsLocalController())
    {
        Server_RequestAllCategoriesVerbosity();
        Server_SetSelection(FCogDebugSettings::GetSelection());
        Server_SetIsFilteringBySelection(FCogDebugSettings::GetIsFilteringBySelection());
    }
}

//--------------------------------------------------------------------------------------------------------------------------
void ACogDebugReplicator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS_FAST(ACogDebugReplicator, ReplicatedData, Params);
}
 
//--------------------------------------------------------------------------------------------------------------------------
void ACogDebugReplicator::TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
#if !UE_BUILD_SHIPPING

    Super::TickActor(DeltaTime, TickType, ThisTickFunction);
    if (OwnerPlayerController)
    {
        if (GetWorld()->GetNetMode() == NM_Client)
        {
            for (const FCogDebugShape& ReplicatedShape : ReplicatedShapes)
            {
                ReplicatedShape.Draw(GetWorld());
            }
        }
    }

#endif // !UE_BUILD_SHIPPING
}

//--------------------------------------------------------------------------------------------------------------------------
void ACogDebugReplicator::Server_SetCategoryVerbosity_Implementation(FName LogCategoryName, ECogLogVerbosity Verbosity)
{
#if !UE_BUILD_SHIPPING

    ENetMode NetMode = GetWorld()->GetNetMode();
    if (NetMode == NM_DedicatedServer || NetMode == NM_ListenServer)
    {
        if (FCogDebugLogCategoryInfo* LogCategoryInfo = FCogDebugLog::FindLogCategoryInfo(LogCategoryName))
        {
            LogCategoryInfo->LogCategory->SetVerbosity((ELogVerbosity::Type)Verbosity);

            TArray<FCogServerCategoryData> CategoriesData;
            CategoriesData.Add({ LogCategoryName, Verbosity });
            NetMulticast_SendCategoriesVerbosity(CategoriesData);
        }
    }

#endif // !UE_BUILD_SHIPPING
}

//--------------------------------------------------------------------------------------------------------------------------
void ACogDebugReplicator::NetMulticast_SendCategoriesVerbosity_Implementation(const TArray<FCogServerCategoryData>& Categories)
{
#if !UE_BUILD_SHIPPING

    if (GetWorld()->GetNetMode() == NM_Client)
    {
        for (const FCogServerCategoryData& Category : Categories)
        {
            FCogDebugLog::OnServerVerbosityChanged(Category.LogCategoryName, (ELogVerbosity::Type)Category.Verbosity);
        }
    }

#endif // !UE_BUILD_SHIPPING
}

//--------------------------------------------------------------------------------------------------------------------------
void ACogDebugReplicator::Client_SendCategoriesVerbosity_Implementation(const TArray<FCogServerCategoryData>& Categories)
{
#if !UE_BUILD_SHIPPING

    if (GetWorld()->GetNetMode() == NM_Client)
    {
        for (const FCogServerCategoryData& Category : Categories)
        {
            FCogDebugLog::OnServerVerbosityChanged(Category.LogCategoryName, (ELogVerbosity::Type)Category.Verbosity);
        }
    }

#endif // !UE_BUILD_SHIPPING
}

//--------------------------------------------------------------------------------------------------------------------------
void ACogDebugReplicator::Server_RequestAllCategoriesVerbosity_Implementation()
{
#if !UE_BUILD_SHIPPING

    ENetMode NetMode = GetWorld()->GetNetMode();
    if (NetMode == NM_DedicatedServer || NetMode == NM_ListenServer)
    {
        TArray<FCogServerCategoryData> CategoriesData;
        for (auto& Entry : FCogDebugLog::GetLogCategories())
        {
            FCogDebugLogCategoryInfo& CategoryInfo = Entry.Value;
            if (CategoryInfo.LogCategory != nullptr)
            {
                CategoriesData.Add(
                { 
                    CategoryInfo.LogCategory->GetCategoryName(), 
                    (ECogLogVerbosity)CategoryInfo.LogCategory->GetVerbosity() 
                });
            }
        }

        Client_SendCategoriesVerbosity(CategoriesData);
    }

#endif // !UE_BUILD_SHIPPING
}

//--------------------------------------------------------------------------------------------------------------------------
void ACogDebugReplicator::Server_SetIsFilteringBySelection_Implementation(bool Value)
{
#if !UE_BUILD_SHIPPING

    bIsServerFilteringBySelection = Value;

#endif // !UE_BUILD_SHIPPING
}

//--------------------------------------------------------------------------------------------------------------------------
void ACogDebugReplicator::Server_SetSelection_Implementation(AActor* Value)
{
#if !UE_BUILD_SHIPPING

    ServerSelection = Value;

#endif // !UE_BUILD_SHIPPING
}

//--------------------------------------------------------------------------------------------------------------------------
// FCogReplicatorNetPack
//--------------------------------------------------------------------------------------------------------------------------
class FCogReplicatorNetState : public INetDeltaBaseState
{
public:

    virtual bool IsStateEqual(INetDeltaBaseState* OtherState) override
    {
        FCogReplicatorNetState* Other = static_cast<FCogReplicatorNetState*>(OtherState);
        return (ShapesRepCounter == Other->ShapesRepCounter);
    }

    int32 ShapesRepCounter = 0;
};

//--------------------------------------------------------------------------------------------------------------------------
// FCogReplicatorNetPack
//--------------------------------------------------------------------------------------------------------------------------
bool FCogReplicatorNetPack::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
{
    if (DeltaParms.bUpdateUnmappedObjects || Owner == nullptr)
    {
        return true;
    }

    if (DeltaParms.Writer)
    {
        const bool bIsOwnerClient = !Owner->bHasAuthority;
        if (bIsOwnerClient)
        {
            return false;
        }

        FCogReplicatorNetState* OldState = static_cast<FCogReplicatorNetState*>(DeltaParms.OldState);
        FCogReplicatorNetState* NewState = new FCogReplicatorNetState();
        check(DeltaParms.NewState);
        *DeltaParms.NewState = TSharedPtr<INetDeltaBaseState>(NewState);

        //------------------------------------------------------------------------------------------------------------------
        // Find delta to replicate
        //------------------------------------------------------------------------------------------------------------------
        {
            const bool bMissingOldState = (OldState == nullptr);
            const bool bShapesChanged = (SavedShapes != Owner->ReplicatedShapes);
            NewState->ShapesRepCounter = (bMissingOldState ? 0 : OldState->ShapesRepCounter) + (bShapesChanged ? 1 : 0);
            if (bShapesChanged)
            {
                SavedShapes = Owner->ReplicatedShapes;
                Owner->ReplicatedShapes.Empty();
            }
        }

        //------------------------------------------------------------------------------------------------------------------
        // Write
        //------------------------------------------------------------------------------------------------------------------
        {
            const bool bMissingOldState = (OldState == nullptr);
            const uint8 ShouldUpdateShapes = bMissingOldState || (OldState->ShapesRepCounter != NewState->ShapesRepCounter);

            FBitWriter& Writer = *DeltaParms.Writer;
            Writer.WriteBit(ShouldUpdateShapes);
            if (ShouldUpdateShapes)
            {
                Writer << SavedShapes;
            }
        }
    }
    else if (DeltaParms.Reader)
    {
        //------------------------------------------------------------------------------------------------------------------
        // Read
        //------------------------------------------------------------------------------------------------------------------
        FBitReader& Reader = *DeltaParms.Reader;
        const uint8 ShouldUpdateShapes = Reader.ReadBit();
        if (ShouldUpdateShapes)
        {
            Reader << Owner->ReplicatedShapes;
        }
    }

    return true;
}
