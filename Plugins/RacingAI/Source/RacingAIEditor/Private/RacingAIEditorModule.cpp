#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "SRaceSetupPanel.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SNullWidget.h"

namespace
{
const FName RaceSetupTabName(TEXT("RacingAI.RaceSetup"));
}

class FRacingAIEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (IsRunningCommandlet())
		{
			return;
		}

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(RaceSetupTabName,
			FOnSpawnTab::CreateRaw(this, &FRacingAIEditorModule::SpawnTab))
			.SetDisplayName(FText::FromString(TEXT("Race Setup")))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FRacingAIEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		if (IsRunningCommandlet())
		{
			return;
		}

		UToolMenus::UnRegisterStartupCallback(this);
		if (UToolMenus::IsToolMenuUIEnabled())
		{
			UToolMenus::UnregisterOwner(this);
		}

		if (TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->FindExistingLiveTab(RaceSetupTabName))
		{
			Tab->SetContent(SNullWidget::NullWidget);
			Tab->RequestCloseTab();
		}
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RaceSetupTabName);
	}

private:
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)
		[
			SNew(SRaceSetupPanel)
		];
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		Menu->FindOrAddSection(TEXT("RacingAI")).AddMenuEntry(
			TEXT("OpenRaceSetup"),
			FText::FromString(TEXT("Race Setup")),
			FText::FromString(TEXT("GridSpawner의 AI 구성을 프리셋으로 바꿉니다.")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]
			{
				FGlobalTabmanager::Get()->TryInvokeTab(RaceSetupTabName);
			})));
	}
};

IMPLEMENT_MODULE(FRacingAIEditorModule, RacingAIEditor)
