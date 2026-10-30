#pragma once

#include <ISmmPlugin.h>
#include <eiface.h>
#include <engine/igameeventsystem.h>
#include <entity2/entitysystem.h>
#include <igameevents.h>
#include <networksystem/inetworkmessages.h>
#include <schemasystem/schemasystem.h>

#include <string>

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

	const char* GetAuthor() override { return "snowboy"; }
	const char* GetName() override { return "Snowball Round"; }
	const char* GetDescription() override { return "Every tenth round is a knife and snowball battle"; }
	const char* GetURL() override { return ""; }
	const char* GetLicense() override { return "MIT"; }
	const char* GetVersion() override { return "1.0.0"; }
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

	bool RefreshEntitySystem();
	bool ResolveSchemaOffsets();
	bool ReadGameRules(int& currentRound, bool& warmup);
	CEntityInstance* ResolveEntity(CEntityIndex index) const;
	CEntityInstance* ResolveEntity(const CEntityHandle& handle) const;
	int FindSchemaOffset(const char* className, const char* fieldName) const;

	bool m_snowballRound = false;
	bool m_applyLoadoutsNextFrame = false;
	bool m_showMessageNextFrame = false;
	bool m_cvarsOverridden = false;
	bool m_schemaErrorLogged = false;
	int m_fallbackRound = 0;
	int m_currentRound = 0;
	int m_lastEventTick = -1;
	std::string m_lastEventName;
	float m_savedBuyTime = 0.0f;
	int m_savedInfiniteAmmo = 0;

	struct SchemaOffsets
	{
		int playerPawn = -1;
		int pawnAlive = -1;
		int teamNumber = -1;
		int itemServices = -1;
		int gameRules = -1;
		int totalRoundsPlayed = -1;
		int warmupPeriod = -1;
		bool resolved = false;
	} m_offsets;
};

extern SnowballRoundPlugin g_SnowballRound;

PLUGIN_GLOBALVARS();
