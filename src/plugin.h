#pragma once

#include <ISmmPlugin.h>
#include <eiface.h>
#include <engine/igameeventsystem.h>
#include <entity2/entitysystem.h>
#include <igameevents.h>
#include <networksystem/inetworkmessages.h>
#include <schemasystem/schemasystem.h>

#include <string>
#include <vector>
#include "round_tracker.h"

class SnowballRoundPlugin final : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late) override;
	bool Unload(char* error, size_t maxlen) override;

	void OnLevelInit(const char* mapName, const char* mapEntities, const char* oldLevel,
		const char* landmarkName, bool loadGame, bool background) override;
	void OnLevelShutdown() override;

	void HookGameFrame(bool simulating, bool firstTick, bool lastTick);
	void HookClientCommand(CPlayerSlot slot, const CCommand& command);
	void HookPostEvent(CSplitScreenSlot slot, bool localOnly, int clientCount,
		const uint64* clients, INetworkMessageInternal* event, const CNetMessage* data,
		unsigned long size, NetChannelBufType_t bufferType);
	void BeginTestRound();

	const char* GetAuthor() override { return "snowboy"; }
	const char* GetName() override { return "Snowball Round"; }
	const char* GetDescription() override { return "Every tenth round is a knife and snowball battle"; }
	const char* GetURL() override { return ""; }
	const char* GetLicense() override { return "MIT"; }
	const char* GetVersion() override { return "1.1.2"; }
	const char* GetDate() override { return __DATE__; }
	const char* GetLogTag() override { return "SNOWBALL"; }

private:
	void HandleEvent(const char* name);
	void BeginRound();
	void SetSnowballRound(bool enabled);
	void OverrideRoundCvars();
	void RestoreRoundCvars();
	void ApplySnowballLoadoutToAll();
	void ShowRoundMessage();
	void ResetMapState();
	void LockWeapons();
	void RestoreWeapons();
	bool UpdateRoundStateFromRules();
	bool ResolveDecoyGiveFunction();
	using GiveItemFunction = void* (*)(void*, const char*, void*, void*, void*, void*);
	GiveItemFunction m_giveItem = nullptr;
	bool m_giveItemResolved = false;
	bool m_loadoutErrorLogged = false;
	int m_loadoutFrames = 0;

	bool RefreshEntitySystem();
	bool ResolveSchemaOffsets();
	bool ReadGameRules(int& currentRound, bool& warmup, RoundSnapshot* snapshot = nullptr);
	CEntityInstance* ResolveEntity(CEntityIndex index) const;
	CEntityInstance* ResolveEntity(const CEntityHandle& handle) const;
	int FindSchemaOffset(const char* className, const char* fieldName) const;

	bool m_snowballRound = false;
	bool m_applyLoadoutsNextFrame = false;
	bool m_showMessageNextFrame = false;
	bool m_cvarsOverridden = false;
	bool m_schemaErrorLogged = false;
	bool m_roundPrepared = false;
	RoundTracker m_roundTracker;
	int m_fallbackRound = 0;
	int m_currentRound = 0;
	int m_lastEventTick = -1;
	std::string m_lastEventName;
	float m_savedBuyTime = 0.0f;
	int m_savedInfiniteAmmo = 0;
	int m_savedBuyGuns = 1;
	int m_savedBuyGrenades = 1;
	std::string m_savedTDefaultGrenades;
	std::string m_savedCTDefaultGrenades;
	struct LockedWeapon
	{
		CEntityHandle handle;
		int primary;
		int secondary;
	};
	std::vector<LockedWeapon> m_lockedWeapons;
	int m_primaryAttack = -1;
	int m_secondaryAttack = -1;
	bool m_weaponSchemaErrorLogged = false;

	struct SchemaOffsets
	{
		int playerPawn = -1;
		int pawnAlive = -1;
		int teamNumber = -1;
		int itemServices = -1;
		int ownerEntity = -1;
		int gameRules = -1;
		int totalRoundsPlayed = -1;
		int warmupPeriod = -1;
		int freezePeriod = -1;
		int roundStartTime = -1;
		int restartRoundTime = -1;
		bool resolved = false;
	} m_offsets;
};

extern SnowballRoundPlugin g_SnowballRound;

PLUGIN_GLOBALVARS();
