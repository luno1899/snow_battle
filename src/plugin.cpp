#include "plugin.h"

#include <icvar.h>
#include <iserver.h>
#include <tier1/convar.h>
#include <tier1/strtools.h>

#include "gameevents.pb.h"
#include "usermessages.pb.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace
{
constexpr int kSnowballInterval = 10;
constexpr int kHudPrintCenter = 4;
constexpr int kTeamTerrorist = 2;
constexpr int kTeamCounterTerrorist = 3;
constexpr int kEntitySystemOffsetWindows = 88;
constexpr int kEntitySystemOffsetLinux = 80;
constexpr const char* kRoundMessage =
	"СНЕЖНАЯ БИТВА! Только ножи и бесконечные снежки!";

IServerGameDLL* g_server = nullptr;
IServerGameClients* g_gameClients = nullptr;
ICvar* g_cvar = nullptr;
ISchemaSystem* g_schemaSystem = nullptr;
IGameResourceService* g_gameResourceService = nullptr;
IGameEventSystem* g_gameEventSystem = nullptr;
INetworkMessages* g_networkMessages = nullptr;
CGameEntitySystem* g_entitySystem = nullptr;

class CPlayerPawnComponent
{
public:
	virtual ~CPlayerPawnComponent() = 0;
	virtual void Unknown01() = 0;
	virtual void Unknown02() = 0;
	virtual void Unknown03() = 0;
	virtual void Unknown04() = 0;
	virtual void Unknown05() = 0;
	virtual void Unknown06() = 0;
	virtual void Unknown07() = 0;
	virtual void Unknown08() = 0;
	virtual void Unknown09() = 0;
	virtual void Unknown10() = 0;
	virtual void Unknown11() = 0;
	virtual void Unknown12() = 0;
	virtual void Unknown13() = 0;
	virtual void Unknown14() = 0;
	virtual void Unknown15() = 0;
	virtual void Unknown16() = 0;
	virtual void Unknown17() = 0;
	virtual void Unknown18() = 0;
	virtual void Unknown19() = 0;
	virtual void Unknown20() = 0;
	virtual void Unknown21() = 0;
	virtual void Unknown22() = 0;
};

class CPlayerItemServices : public CPlayerPawnComponent
{
public:
	virtual ~CPlayerItemServices() = 0;
};

class CCSPlayerItemServices : public CPlayerItemServices
{
private:
	virtual void* GiveNamedItemInternal(const char* name) = 0;

public:
	virtual bool GiveNamedItemBool(const char* name) = 0;
	virtual void* GiveNamedItem(const char* name) = 0;
	virtual void DropActiveWeapon(void* weapon) = 0;
	virtual void StripPlayerWeapons(bool removeSuit) = 0;
};

bool IsBuyCommand(const char* command)
{
	return command &&
		(!V_stricmp(command, "buy") || !V_stricmp(command, "autobuy") ||
		 !V_stricmp(command, "rebuy") || !V_stricmp(command, "buyammo1") ||
		 !V_stricmp(command, "buyammo2"));
}
} // namespace

SH_DECL_HOOK3_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);
SH_DECL_HOOK2_void(IServerGameClients, ClientCommand, SH_NOATTRIB, 0, CPlayerSlot, const CCommand&);
SH_DECL_HOOK8_void(IGameEventSystem, PostEventAbstract, SH_NOATTRIB, 0,
	CSplitScreenSlot, bool, int, const uint64*, INetworkMessageInternal*,
	const CNetMessage*, unsigned long, NetChannelBufType_t);

SnowballRoundPlugin g_SnowballRound;
PLUGIN_EXPOSE(SnowballRoundPlugin, g_SnowballRound);

bool SnowballRoundPlugin::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_cvar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_schemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_gameResourceService, IGameResourceService, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_server, IServerGameDLL, INTERFACEVERSION_SERVERGAMEDLL);
	GET_V_IFACE_ANY(GetServerFactory, g_gameClients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
	GET_V_IFACE_ANY(GetEngineFactory, g_gameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_networkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);

	g_pCVar = g_cvar;
	g_SMAPI->AddListener(this, this);

	SH_ADD_HOOK(IServerGameDLL, GameFrame, g_server, SH_MEMBER(this, &SnowballRoundPlugin::HookGameFrame), true);
	SH_ADD_HOOK(IServerGameClients, ClientCommand, g_gameClients, SH_MEMBER(this, &SnowballRoundPlugin::HookClientCommand), false);
	SH_ADD_HOOK(IGameEventSystem, PostEventAbstract, g_gameEventSystem, SH_MEMBER(this, &SnowballRoundPlugin::HookPostEvent), false);

	META_CONPRINTF("[SnowballRound] Loaded for Metamod:Source 2.0 build 1411 (SourceHook).\n");
	return true;
}

bool SnowballRoundPlugin::Unload(char* error, size_t maxlen)
{
	RestoreRoundCvars();

	SH_REMOVE_HOOK(IServerGameDLL, GameFrame, g_server, SH_MEMBER(this, &SnowballRoundPlugin::HookGameFrame), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientCommand, g_gameClients, SH_MEMBER(this, &SnowballRoundPlugin::HookClientCommand), false);
	SH_REMOVE_HOOK(IGameEventSystem, PostEventAbstract, g_gameEventSystem, SH_MEMBER(this, &SnowballRoundPlugin::HookPostEvent), false);
	return true;
}

void SnowballRoundPlugin::OnLevelInit(const char*, const char*, const char*, const char*, bool, bool)
{
	ResetMapState();
}

void SnowballRoundPlugin::OnLevelShutdown()
{
	RestoreRoundCvars();
	g_entitySystem = nullptr;
	ResetMapState();
}

void SnowballRoundPlugin::ResetMapState()
{
	m_snowballRound = false;
	m_applyLoadoutsNextFrame = false;
	m_showMessageNextFrame = false;
	m_fallbackRound = 0;
	m_currentRound = 0;
	m_lastEventTick = -1;
	m_lastEventName.clear();
}

void SnowballRoundPlugin::HookGameFrame(bool simulating, bool, bool)
{
	if (!simulating)
		RETURN_META(MRES_IGNORED);

	if (m_applyLoadoutsNextFrame)
	{
		m_applyLoadoutsNextFrame = false;
		if (m_snowballRound)
			ApplySnowballLoadoutToAll();
	}

	if (m_showMessageNextFrame)
	{
		m_showMessageNextFrame = false;
		if (m_snowballRound)
			ShowRoundMessage();
	}

	RETURN_META(MRES_IGNORED);
}

void SnowballRoundPlugin::HookClientCommand(CPlayerSlot, const CCommand& command)
{
	if (m_snowballRound && command.ArgC() > 0 && IsBuyCommand(command[0]))
		RETURN_META(MRES_SUPERCEDE);

	RETURN_META(MRES_IGNORED);
}

void SnowballRoundPlugin::HookPostEvent(CSplitScreenSlot, bool, int, const uint64*,
	INetworkMessageInternal* event, const CNetMessage* data, unsigned long, NetChannelBufType_t)
{
	if (!event || !data)
		RETURN_META(MRES_IGNORED);

	const NetMessageInfo_t* info = event->GetNetMessageInfo();
	if (!info || info->m_MessageId != GE_Source1LegacyGameEvent)
		RETURN_META(MRES_IGNORED);

	const auto* message = data->ToPB<CMsgSource1LegacyGameEvent>();
	if (!message || !message->has_event_name())
		RETURN_META(MRES_IGNORED);

	const int tick = message->has_server_tick() ? message->server_tick() : -1;
	if (tick == m_lastEventTick && m_lastEventName == message->event_name())
		RETURN_META(MRES_IGNORED);

	m_lastEventTick = tick;
	m_lastEventName = message->event_name();
	HandleEvent(message->event_name().c_str());
	RETURN_META(MRES_IGNORED);
}

void SnowballRoundPlugin::HandleEvent(const char* name)
{
	if (!std::strcmp(name, "round_prestart"))
	{
		BeginRound();
	}
	else if (!std::strcmp(name, "round_start"))
	{
		if (m_snowballRound)
		{
			m_applyLoadoutsNextFrame = true;
			m_showMessageNextFrame = true;
		}
	}
	else if (!std::strcmp(name, "player_spawn"))
	{
		if (m_snowballRound)
			m_applyLoadoutsNextFrame = true;
	}
	else if (!std::strcmp(name, "round_announce_match_start"))
	{
		m_fallbackRound = 0;
		m_currentRound = 0;
		SetSnowballRound(false);
	}
}

void SnowballRoundPlugin::BeginRound()
{
	bool warmup = false;
	int round = 0;
	if (!ReadGameRules(round, warmup))
		round = ++m_fallbackRound;
	else
		m_fallbackRound = round;

	m_currentRound = round;
	SetSnowballRound(!warmup && round > 0 && round % kSnowballInterval == 0);

	if (m_snowballRound)
	{
		m_applyLoadoutsNextFrame = true;
		META_CONPRINTF("[SnowballRound] Round %d is a snowball round.\n", m_currentRound);
	}
}

void SnowballRoundPlugin::SetSnowballRound(bool enabled)
{
	if (enabled == m_snowballRound)
		return;

	m_snowballRound = enabled;
	if (enabled)
		OverrideRoundCvars();
	else
		RestoreRoundCvars();
}

void SnowballRoundPlugin::OverrideRoundCvars()
{
	if (m_cvarsOverridden)
		return;

	ConVarRefAbstract buyTime("mp_buytime");
	ConVarRefAbstract infiniteAmmo("sv_infinite_ammo");
	if (!buyTime.IsValidRef() || !infiniteAmmo.IsValidRef())
	{
		META_CONPRINTF("[SnowballRound] Could not find mp_buytime or sv_infinite_ammo.\n");
		return;
	}

	m_savedBuyTime = buyTime.GetFloat();
	m_savedInfiniteAmmo = infiniteAmmo.GetInt();
	m_cvarsOverridden = true;
	buyTime.SetFloat(0.0f);
	infiniteAmmo.SetInt(1);
}

void SnowballRoundPlugin::RestoreRoundCvars()
{
	if (!m_cvarsOverridden)
		return;

	ConVarRefAbstract buyTime("mp_buytime");
	ConVarRefAbstract infiniteAmmo("sv_infinite_ammo");
	if (buyTime.IsValidRef())
		buyTime.SetFloat(m_savedBuyTime);
	if (infiniteAmmo.IsValidRef())
		infiniteAmmo.SetInt(m_savedInfiniteAmmo);
	m_cvarsOverridden = false;
}

bool SnowballRoundPlugin::RefreshEntitySystem()
{
	if (!g_gameResourceService)
		return false;

#ifdef _WIN32
	constexpr int offset = kEntitySystemOffsetWindows;
#else
	constexpr int offset = kEntitySystemOffsetLinux;
#endif
	g_entitySystem = *reinterpret_cast<CGameEntitySystem**>(
		reinterpret_cast<uintptr_t>(g_gameResourceService) + offset);
	return g_entitySystem != nullptr;
}

int SnowballRoundPlugin::FindSchemaOffset(const char* className, const char* fieldName) const
{
#ifdef _WIN32
	constexpr const char* serverModule = "server.dll";
#else
	constexpr const char* serverModule = "libserver.so";
#endif
	CSchemaSystemTypeScope* scope = g_schemaSystem ? g_schemaSystem->FindTypeScopeForModule(serverModule) : nullptr;
	CSchemaClassInfo* classInfo = scope ? scope->FindDeclaredClass(className).Get() : nullptr;
	if (!classInfo)
		return -1;

	for (int i = 0; i < classInfo->m_nFieldCount; ++i)
	{
		const SchemaClassFieldData_t& field = classInfo->m_pFields[i];
		if (field.m_pszName && !std::strcmp(field.m_pszName, fieldName))
			return field.m_nSingleInheritanceOffset;
	}
	return -1;
}

bool SnowballRoundPlugin::ResolveSchemaOffsets()
{
	if (m_offsets.resolved)
		return true;

	m_offsets.playerPawn = FindSchemaOffset("CCSPlayerController", "m_hPlayerPawn");
	m_offsets.pawnAlive = FindSchemaOffset("CCSPlayerController", "m_bPawnIsAlive");
	m_offsets.teamNumber = FindSchemaOffset("CBaseEntity", "m_iTeamNum");
	m_offsets.itemServices = FindSchemaOffset("CBasePlayerPawn", "m_pItemServices");
	m_offsets.gameRules = FindSchemaOffset("CCSGameRulesProxy", "m_pGameRules");
	m_offsets.totalRoundsPlayed = FindSchemaOffset("CCSGameRules", "m_totalRoundsPlayed");
	m_offsets.warmupPeriod = FindSchemaOffset("CCSGameRules", "m_bWarmupPeriod");

	m_offsets.resolved = m_offsets.playerPawn >= 0 && m_offsets.pawnAlive >= 0 &&
		m_offsets.teamNumber >= 0 && m_offsets.itemServices >= 0 && m_offsets.gameRules >= 0 &&
		m_offsets.totalRoundsPlayed >= 0 && m_offsets.warmupPeriod >= 0;
	if (!m_offsets.resolved && !m_schemaErrorLogged)
	{
		m_schemaErrorLogged = true;
		META_CONPRINTF("[SnowballRound] Required CS2 schema fields were not found.\n");
	}
	return m_offsets.resolved;
}

CEntityInstance* SnowballRoundPlugin::ResolveEntity(CEntityIndex index) const
{
	if (!g_entitySystem || index.Get() < 0 || index.Get() >= MAX_TOTAL_ENTITIES)
		return nullptr;

	CEntityIdentity* chunk = g_entitySystem->m_EntityList.m_pIdentityChunks[index.Get() / MAX_ENTITIES_IN_LIST];
	if (!chunk)
		return nullptr;
	CEntityIdentity* identity = &chunk[index.Get() % MAX_ENTITIES_IN_LIST];
	return identity->GetEntityIndex() == index ? identity->m_pInstance : nullptr;
}

CEntityInstance* SnowballRoundPlugin::ResolveEntity(const CEntityHandle& handle) const
{
	if (!g_entitySystem || !handle.IsValid())
		return nullptr;

	const int entry = handle.GetEntryIndex();
	if (entry < 0 || entry >= MAX_TOTAL_ENTITIES)
		return nullptr;
	CEntityIdentity* chunk = g_entitySystem->m_EntityList.m_pIdentityChunks[entry / MAX_ENTITIES_IN_LIST];
	if (!chunk)
		return nullptr;
	CEntityIdentity* identity = &chunk[entry % MAX_ENTITIES_IN_LIST];
	return identity->GetRefEHandle() == handle ? identity->m_pInstance : nullptr;
}

bool SnowballRoundPlugin::ReadGameRules(int& currentRound, bool& warmup)
{
	if (!RefreshEntitySystem() || !ResolveSchemaOffsets())
		return false;

	for (CEntityIdentity* identity = g_entitySystem->m_EntityList.m_pFirstActiveEntity;
		 identity; identity = identity->m_pNext)
	{
		if (!identity->m_pInstance || std::strcmp(identity->GetClassname(), "cs_gamerules"))
			continue;

		auto* proxy = reinterpret_cast<uint8_t*>(identity->m_pInstance);
		void* rules = *reinterpret_cast<void**>(proxy + m_offsets.gameRules);
		if (!rules)
			return false;

		auto* bytes = reinterpret_cast<uint8_t*>(rules);
		currentRound = *reinterpret_cast<int*>(bytes + m_offsets.totalRoundsPlayed) + 1;
		warmup = *reinterpret_cast<bool*>(bytes + m_offsets.warmupPeriod);
		return true;
	}
	return false;
}

void SnowballRoundPlugin::ApplySnowballLoadoutToAll()
{
	if (!RefreshEntitySystem() || !ResolveSchemaOffsets())
		return;

	constexpr int maxClients = 64;
	for (int slot = 0; slot < maxClients; ++slot)
	{
		CEntityInstance* controller = ResolveEntity(CEntityIndex(slot + 1));
		if (!controller)
			continue;

		auto* controllerBytes = reinterpret_cast<uint8_t*>(controller);
		if (!*reinterpret_cast<bool*>(controllerBytes + m_offsets.pawnAlive))
			continue;

		const CEntityHandle pawnHandle = *reinterpret_cast<CEntityHandle*>(controllerBytes + m_offsets.playerPawn);
		CEntityInstance* pawn = ResolveEntity(pawnHandle);
		if (!pawn)
			continue;

		auto* pawnBytes = reinterpret_cast<uint8_t*>(pawn);
		auto* items = *reinterpret_cast<CCSPlayerItemServices**>(pawnBytes + m_offsets.itemServices);
		if (!items)
			continue;

		const uint8_t team = *reinterpret_cast<uint8_t*>(controllerBytes + m_offsets.teamNumber);
		if (team != kTeamTerrorist && team != kTeamCounterTerrorist)
			continue;

		items->StripPlayerWeapons(false);
		items->GiveNamedItem(team == kTeamTerrorist ? "weapon_knife_t" : "weapon_knife");
		items->GiveNamedItem("weapon_decoy");
	}
}

void SnowballRoundPlugin::ShowRoundMessage()
{
	if (!g_networkMessages || !g_gameEventSystem)
		return;

	INetworkMessageInternal* networkMessage = g_networkMessages->FindNetworkMessagePartial("TextMsg");
	if (!networkMessage)
		return;

	auto* data = networkMessage->AllocateMessage()->ToPB<CUserMessageTextMsg>();
	data->set_dest(kHudPrintCenter);
	data->add_param(kRoundMessage);
	g_gameEventSystem->PostEventAbstract(-1, false, -1, nullptr, networkMessage, data, 0, BUF_RELIABLE);
	delete data;
}
