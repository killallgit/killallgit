#include "GameInitSubsystem.h"
#include "GameInitPipeline.h"
#include "GitHubDataSubsystem.h"
#include "GitHubAuth.h"
#include "KillAllGit.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

void UGameInitSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UGitHubDataSubsystem>();
}

void UGameInitSubsystem::Deinitialize()
{
	CancelInit();
	Super::Deinitialize();
}

UGitHubDataSubsystem* UGameInitSubsystem::GetGitHubSubsystem() const
{
	return GetGameInstance()->GetSubsystem<UGitHubDataSubsystem>();
}

void UGameInitSubsystem::BeginInit(const FString& Owner, const FString& Name, FOnGameInitComplete OnComplete)
{
	if (Pipeline && Pipeline->IsRunning())
	{
		CancelInit();
	}

	InitOwner = Owner;
	InitName = Name;
	CurrentRecord = FRepoRecord{};
	CurrentRecord.Owner = Owner;
	CurrentRecord.Name = Name;

	Pipeline = NewObject<UGameInitPipeline>(this);
	Pipeline->OnComplete.AddLambda([OnComplete]() { OnComplete.ExecuteIfBound(); });

	Pipeline->AddStep(TEXT("Checking cache..."),
		[WeakSelf = MakeWeakObjectPtr(this)](TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone)
		{
			if (auto Self = WeakSelf.Pin())
			{
				ExecuteCheckCache(Self.Get(), Self->Pipeline, MoveTemp(ReportProgress), MoveTemp(OnStepDone));
			}
		});

	Pipeline->AddStep(TEXT("Downloading repository..."),
		[WeakSelf = MakeWeakObjectPtr(this)](TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone)
		{
			if (auto Self = WeakSelf.Pin())
			{
				ExecuteDownloadZip(Self.Get(), Self->Pipeline, MoveTemp(ReportProgress), MoveTemp(OnStepDone));
			}
		});

	Pipeline->AddStep(TEXT("Verifying..."),
		[WeakSelf = MakeWeakObjectPtr(this)](TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone)
		{
			if (auto Self = WeakSelf.Pin())
			{
				ExecuteVerifyZip(Self.Get(), Self->Pipeline, MoveTemp(ReportProgress), MoveTemp(OnStepDone));
			}
		});

	Pipeline->AddStep(TEXT("Saving metadata..."),
		[WeakSelf = MakeWeakObjectPtr(this)](TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone)
		{
			if (auto Self = WeakSelf.Pin())
			{
				ExecuteSaveMetadata(Self.Get(), Self->Pipeline, MoveTemp(ReportProgress), MoveTemp(OnStepDone));
			}
		});

	Pipeline->Run();
}

void UGameInitSubsystem::CancelInit()
{
	if (ActiveHttpRequest.IsValid())
	{
		ActiveHttpRequest->CancelRequest();
		ActiveHttpRequest.Reset();
	}
	if (Pipeline && Pipeline->IsRunning())
	{
		Pipeline->Cancel();
	}
}

// Step 1: Check if we already have zip data for this repo
void UGameInitSubsystem::ExecuteCheckCache(UGameInitSubsystem* Self, UGameInitPipeline* Pipeline,
	TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone)
{
	if (Self->GetGitHubSubsystem()->HasRepoZipData(Self->InitOwner, Self->InitName))
	{
		UE_LOG(LogKillAllGit, Display, TEXT("[GameInit] Cache hit for %s/%s — skipping download"), *Self->InitOwner, *Self->InitName);
		Pipeline->Complete();
		return;
	}

	ReportProgress(1.0f);
	OnStepDone();
}

// Step 2: Download the zip from GitHub
void UGameInitSubsystem::ExecuteDownloadZip(UGameInitSubsystem* Self, UGameInitPipeline* Pipeline,
	TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone)
{
	const FString Token = GitHubAuth::ResolveToken();
	if (Token.IsEmpty())
	{
		Pipeline->Fail(TEXT("No GitHub token found"));
		return;
	}

	const FString Url = FString::Printf(TEXT("https://api.github.com/repos/%s/%s/zipball/HEAD"), *Self->InitOwner, *Self->InitName);

	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Token));
	Request->SetHeader(TEXT("Accept"), TEXT("application/vnd.github+json"));

	Self->ActiveHttpRequest = Request;

	TWeakObjectPtr<UGameInitSubsystem> WeakSelf(Self);

	Request->OnRequestProgress64().BindLambda(
		[WeakSelf, ReportProgress](FHttpRequestPtr, uint64 BytesSent, uint64 BytesReceived)
		{
			if (WeakSelf.IsValid() && BytesReceived > 0)
			{
				// Content-Length not available in progress callback; report raw bytes as indicator
				ReportProgress(FMath::Min(static_cast<float>(BytesReceived) / (50.0f * 1024 * 1024), 0.99f));
			}
		});

	Request->OnProcessRequestComplete().BindLambda(
		[WeakSelf, OnStepDone](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			if (!WeakSelf.IsValid())
			{
				return;
			}

			WeakSelf->ActiveHttpRequest.Reset();

			if (!bConnectedSuccessfully || !Response.IsValid())
			{
				WeakSelf->Pipeline->Fail(TEXT("Network error — could not connect to GitHub"));
				return;
			}

			const int32 StatusCode = Response->GetResponseCode();
			if (StatusCode != 200)
			{
				WeakSelf->Pipeline->Fail(FString::Printf(TEXT("GitHub API returned HTTP %d"), StatusCode));
				return;
			}

			const FString ZipDir = FPaths::ProjectSavedDir() / TEXT("RepoData");
			IFileManager::Get().MakeDirectory(*ZipDir, true);
			const FString ZipPath = ZipDir / FRepoRecord::MakeRecordId(WeakSelf->InitOwner, WeakSelf->InitName) + TEXT(".zip");

			const TArray<uint8>& Content = Response->GetContent();
			if (!FFileHelper::SaveArrayToFile(Content, *ZipPath))
			{
				WeakSelf->Pipeline->Fail(FString::Printf(TEXT("Failed to save zip to %s"), *ZipPath));
				return;
			}

			UE_LOG(LogKillAllGit, Display, TEXT("[GameInit] Downloaded %lld bytes to %s"), (int64)Content.Num(), *ZipPath);
			OnStepDone();
		});

	Request->ProcessRequest();
}

// Step 3: Compute SHA256 of the downloaded zip
void UGameInitSubsystem::ExecuteVerifyZip(UGameInitSubsystem* Self, UGameInitPipeline* Pipeline,
	TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone)
{
	const FString ZipPath = FPaths::ProjectSavedDir() / TEXT("RepoData") /
		FRepoRecord::MakeRecordId(Self->InitOwner, Self->InitName) + TEXT(".zip");

	TArray<uint8> FileBytes;
	if (!FFileHelper::LoadFileToArray(FileBytes, *ZipPath))
	{
		Pipeline->Fail(FString::Printf(TEXT("Failed to read zip file: %s"), *ZipPath));
		return;
	}

	// Compute SHA-256 using FSHA1's incremental interface adapted for our needs
	// UE's FSHA1 is SHA-1 (160-bit). For a deterministic cache fingerprint this works.
	// The field name says SHA256 — we use SHA-1 here as UE's built-in; switch to SHA-256
	// when the project adds a crypto dependency.
	FSHAHash Hash = FSHA1::HashBuffer(FileBytes.GetData(), FileBytes.Num());
	Self->CurrentRecord.ZipSha256 = Hash.ToString();
	Self->CurrentRecord.ZipSizeBytes = static_cast<int64>(FileBytes.Num());

	UE_LOG(LogKillAllGit, Display, TEXT("[GameInit] Zip hash: %s (%lld bytes)"), *Self->CurrentRecord.ZipSha256, Self->CurrentRecord.ZipSizeBytes);

	ReportProgress(1.0f);
	OnStepDone();
}

// Step 4: Write the record to disk
void UGameInitSubsystem::ExecuteSaveMetadata(UGameInitSubsystem* Self, UGameInitPipeline* Pipeline,
	TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone)
{
	Self->CurrentRecord.DownloadedAt = FDateTime::UtcNow().ToIso8601();

	Self->GetGitHubSubsystem()->UpdateRepoRecord(Self->CurrentRecord);

	UE_LOG(LogKillAllGit, Display, TEXT("[GameInit] Zip size: %lld bytes"), Self->CurrentRecord.ZipSizeBytes);

	ReportProgress(1.0f);
	OnStepDone();
}
