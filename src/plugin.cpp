#include "plugin.h"
#include "weapon_policy.h"

#include <icvar.h>
#include <iserver.h>
#include <tier1/convar.h>
#include <tier1/strtools.h>

#include "gameevents.pb.h"
#include "usermessages.pb.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>

namespace
{
constexpr int kHudPrintCenter = 4;
constexpr int kTeamTerrorist = 2;
constexpr int kTeamCounterTerrorist = 3;
constexpr int kEntitySystemOffsetWindows = 88;
constexpr int kEntitySystemOffsetLinux = 80;
constexpr const char* kRoundMessage =
	"СНЕЖНАЯ БИТВА! Только ножи и бесконечные снежки!";

IServerGameDLL* g_server = nullptr;
IVEngineServer2* g_engine = nullptr;
IServerGameClients* g_gameClients = nullptr;
ICvar* g_cvar = nullptr;
ISchemaSystem* g_schemaSystem = nullptr;
IGameResourceService* g_gameResourceService = nullptr;
IGameEventSystem* g_gameEventSystem = nullptr;
INetworkMessages* g_networkMessages = nullptr;
CGameEntitySystem* g_entitySystem = nullptr;

bool IsBuyCommand(const char* command)
{
	return command &&
		(!V_stricmp(command, "buy") || !V_stricmp(command, "autobuy") ||
		 !V_stricmp(command, "rebuy") || !V_stricmp(command, "buyammo1") ||
		 !V_stricmp(command, "buyammo2"));
}

bool IsWeaponBlockCommand(const CCommand& command)
{
	if (command.ArgC() < 1)
		return false;
	if (!V_stricmp(command[0], "slot1") || !V_stricmp(command[0], "slot2"))
		return true;
	if (V_stricmp(command[0], "use") || command.ArgC() < 2)
		return false;
	return !V_stricmp(command[1], "weapon_ak47") || !V_stricmp(command[1], "weapon_m4a1") ||
		!V_stricmp(command[1], "weapon_m4a1_silencer") || !V_stricmp(command[1], "weapon_awp") ||
		!V_stricmp(command[1], "weapon_glock") || !V_stricmp(command[1], "weapon_hkp2000") ||
		!V_stricmp(command[1], "weapon_usp_silencer") || !V_stricmp(command[1], "weapon_deagle");
}

void SnowballConsoleCommand(const CCommand&)
{
	g_SnowballRound.BeginTestRound();
}

bool IsSnowballTestCommand(const CCommand& command)
{
	if (command.ArgC() < 1)
		return false;

	// Some CS2 chat/command bridges strip the leading '!' and invoke the
	// handler as a standalone command ("sb" or "mm_sb").
	if (!V_stricmp(command[0], "sb") || !V_stricmp(command[0], "!sb") ||
		!V_stricmp(command[0], "mm_sb") || !V_stricmp(command[0], "snowball") ||
		!V_stricmp(command[0], "!snowball"))
		return true;

	if (command.ArgC() < 2 ||
		(V_stricmp(command[0], "say") && V_stricmp(command[0], "say_team")))
		return false;

	char text[64];
	V_strncpy(text, command[1], sizeof(text));
	const size_t length = std::strlen(text);
	if (length >= 2 && text[0] == '"' && text[length - 1] == '"')
	{
		std::memmove(text, text + 1, length - 2);
		text[length - 2] = '\0';
	}

	return !V_stricmp(text, "!sb") || !V_stricmp(text, "!snowball");
}
} // namespace

SH_DECL_HOOK3_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);
SH_DECL_HOOK2_void(IServerGameClients, ClientCommand, SH_NOATTRIB, 0, CPlayerSlot, const CCommand&);
SH_DECL_HOOK8_void(IGameEventSystem, PostEventAbstract, SH_NOATTRIB, 0,
	CSplitScreenSlot, bool, int, const uint64*, INetworkMessageInternal*,
	const CNetMessage*, unsigned long, NetChannelBufType_t);

SnowballRoundPlugin g_SnowballRound;
ConCommandCreation_t g_snowballConsoleCommand{};
PLUGIN_EXPOSE(SnowballRoundPlugin, g_SnowballRound);

bool SnowballRoundPlugin::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_cvar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_schemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_gameResourceService, IGameResourceService, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_server, IServerGameDLL, INTERFACEVERSION_SERVERGAMEDLL);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_gameClients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
	GET_V_IFACE_ANY(GetEngineFactory, g_gameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_networkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);

	g_pCVar = g_cvar;
	g_snowballConsoleCommand.m_pszName = "mm_sb";
	g_snowballConsoleCommand.m_pszHelpString = "Start a snowball round immediately";
	g_snowballConsoleCommand.m_nFlags = FCVAR_GAMEDLL;
	g_snowballConsoleCommand.m_CBInfo = ConCommandCallbackInfo_t(SnowballConsoleCommand);
	g_cvar->RegisterConCommand(g_snowballConsoleCommand);
	g_SMAPI->AddListener(this, this);

	SH_ADD_HOOK(IServerGameDLL, GameFrame, g_server, SH_MEMBER(this, &SnowballRoundPlugin::HookGameFrame), false);
	SH_ADD_HOOK(IServerGameClients, ClientCommand, g_gameClients, SH_MEMBER(this, &SnowballRoundPlugin::HookClientCommand), false);
	SH_ADD_HOOK(IGameEventSystem, PostEventAbstract, g_gameEventSystem, SH_MEMBER(this, &SnowballRoundPlugin::HookPostEvent), false);

	META_CONPRINTF("[SnowballRound] Loaded for Metamod:Source 2.0 build 1411 (SourceHook).\n");
	return true;
}

bool SnowballRoundPlugin::Unload(char* error, size_t maxlen)
{
	SetSnowballRound(false);

	SH_REMOVE_HOOK(IServerGameDLL, GameFrame, g_server, SH_MEMBER(this, &SnowballRoundPlugin::HookGameFrame), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientCommand, g_gameClients, SH_MEMBER(this, &SnowballRoundPlugin::HookClientCommand), false);
	SH_REMOVE_HOOK(IGameEventSystem, PostEventAbstract, g_gameEventSystem, SH_MEMBER(this, &SnowballRoundPlugin::HookPostEvent), false);
	return true;
}

void SnowballRoundPlugin::OnLevelInit(const char*, const char*, const char*, const char*, bool, bool)
{
	RestoreRoundCvars();
	ResetMapState();
}

void SnowballRoundPlugin::OnLevelShutdown()
{
	RestoreRoundCvars();
	m_lockedWeapons.clear(); // The old map's entities may already be destroyed.
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
	m_roundPrepared = false;
	m_roundTracker = {};
	m_lockedWeapons.clear();
	m_offsets = {};
	m_primaryAttack = m_secondaryAttack = -1;
	m_schemaErrorLogged = m_weaponSchemaErrorLogged = false;
}

void SnowballRoundPlugin::HookGameFrame(bool simulating, bool, bool)
{
	if (!simulating)
		RETURN_META(MRES_IGNORED);

	int round = 0;
	bool warmup = false;
	RoundSnapshot snapshot;
	if (ReadGameRules(round, warmup, &snapshot))
	{
		const RoundChange change = m_roundTracker.Observe(snapshot);
		if (snapshot.restartTime > 0 || change == RoundChange::Ended)
			SetSnowballRound(false);
		else if (change == RoundChange::Started)
		{
			SetSnowballRound(false);
			m_currentRound = round;
			m_fallbackRound = round;
			if (m_roundTracker.ClaimAutomaticRound(round, warmup))
			{
				SetSnowballRound(true);
				m_applyLoadoutsNextFrame = m_showMessageNextFrame = true;
			}
			META_CONPRINTF("[SnowballRound] Rules round %d: snowball=%d, freeze=%d.\n",
				round, m_snowballRound ? 1 : 0, snapshot.freeze ? 1 : 0);
		}
		else if (m_snowballRound && m_currentRound > 0 && round != m_currentRound)
			SetSnowballRound(false);
	}
	if (m_snowballRound)
		LockWeapons();

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
	if (IsSnowballTestCommand(command))
	{
		BeginTestRound();
		RETURN_META(MRES_IGNORED);
	}

	if (m_snowballRound && command.ArgC() > 0 && IsBuyCommand(command[0]))
		RETURN_META(MRES_SUPERCEDE);
	if (m_snowballRound && IsWeaponBlockCommand(command))
		RETURN_META(MRES_SUPERCEDE);

	RETURN_META(MRES_IGNORED);
}

void SnowballRoundPlugin::BeginTestRound()
{
	bool warmup = false;
	RoundSnapshot snapshot;
	if (ReadGameRules(m_currentRound, warmup, &snapshot))
		m_roundTracker.Observe(snapshot);
	SetSnowballRound(true);
	m_applyLoadoutsNextFrame = true;
	m_showMessageNextFrame = true;
	META_CONPRINTF("[SnowballRound] Test snowball round started by !sb.\n");
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
	if (!std::strcmp(name, "round_prestart") || !std::strcmp(name, "round_start") ||
		!std::strcmp(name, "round_end") || !std::strcmp(name, "round_officially_ended"))
	{
		int round = 0;
		bool warmup = false;
		RoundSnapshot snapshot;
		// A single rules observer owns transitions when schema is available.
		// Events can arrive before/after the rules update, or more than once.
		if (ReadGameRules(round, warmup, &snapshot))
			return;
	}
	if (!std::strcmp(name, "round_prestart"))
	{
		SetSnowballRound(false);
		BeginRound();
		m_roundPrepared = true;
	}
	else if (!std::strcmp(name, "round_start"))
	{
		// Some CS2 builds do not emit round_prestart through the legacy event
		// bridge. round_start is always delivered, so use it as the fallback
		// round boundary as well.
		if (!m_roundPrepared)
		{
			SetSnowballRound(false);
			BeginRound();
		}
		m_roundPrepared = false;
		if (m_snowballRound)
		{
			m_applyLoadoutsNextFrame = true;
			m_showMessageNextFrame = true;
		}
	}
	else if (!std::strcmp(name, "round_end") || !std::strcmp(name, "round_officially_ended"))
	{
		SetSnowballRound(false);
		m_roundPrepared = false;
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
		m_roundTracker = {};
		SetSnowballRound(false);
	}
}

void SnowballRoundPlugin::BeginRound()
{
	bool warmup = false;
	int round = 0;
	const bool haveRules = ReadGameRules(round, warmup);
	if (!haveRules)
		round = ++m_fallbackRound;
	else
		m_fallbackRound = round;

	m_currentRound = round;
	if (m_roundTracker.ClaimAutomaticRound(round, warmup))
		SetSnowballRound(true);

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
	{
		m_applyLoadoutsNextFrame = false;
		m_showMessageNextFrame = false;
		RestoreWeapons();
		RestoreRoundCvars();
		META_CONPRINTF("[SnowballRound] Snowball round ended; weapon attacks and cvars restored.\n");
	}
}

void SnowballRoundPlugin::OverrideRoundCvars()
{
	if (m_cvarsOverridden)
		return;

	ConVarRefAbstract buyTime("mp_buytime");
	ConVarRefAbstract infiniteAmmo("sv_infinite_ammo");
	ConVarRefAbstract buyGuns("mp_buy_allow_guns");
	ConVarRefAbstract buyGrenades("mp_buy_allow_grenades");
	ConVarRefAbstract tGrenades("mp_t_default_grenades");
	ConVarRefAbstract ctGrenades("mp_ct_default_grenades");
	if (!buyTime.IsValidRef() || !infiniteAmmo.IsValidRef())
	{
		META_CONPRINTF("[SnowballRound] Could not find mp_buytime or sv_infinite_ammo.\n");
		return;
	}

	m_savedBuyTime = buyTime.GetFloat();
	m_savedInfiniteAmmo = infiniteAmmo.GetInt();
	if (buyGuns.IsValidRef())
		m_savedBuyGuns = buyGuns.GetInt();
	if (buyGrenades.IsValidRef())
		m_savedBuyGrenades = buyGrenades.GetInt();
	if (tGrenades.IsValidRef())
		m_savedTDefaultGrenades = tGrenades.GetString().Get();
	if (ctGrenades.IsValidRef())
		m_savedCTDefaultGrenades = ctGrenades.GetString().Get();
	m_cvarsOverridden = true;
	buyTime.SetFloat(0.0f);
	infiniteAmmo.SetInt(1);
	if (buyGuns.IsValidRef())
		buyGuns.SetInt(0);
	if (buyGrenades.IsValidRef())
		buyGrenades.SetInt(1);
	if (tGrenades.IsValidRef())
		tGrenades.SetString("weapon_decoy");
	if (ctGrenades.IsValidRef())
		ctGrenades.SetString("weapon_decoy");
}

void SnowballRoundPlugin::RestoreRoundCvars()
{
	if (!m_cvarsOverridden)
		return;

	ConVarRefAbstract buyTime("mp_buytime");
	ConVarRefAbstract infiniteAmmo("sv_infinite_ammo");
	ConVarRefAbstract buyGuns("mp_buy_allow_guns");
	ConVarRefAbstract buyGrenades("mp_buy_allow_grenades");
	ConVarRefAbstract tGrenades("mp_t_default_grenades");
	ConVarRefAbstract ctGrenades("mp_ct_default_grenades");
	if (buyTime.IsValidRef())
		buyTime.SetFloat(m_savedBuyTime);
	if (infiniteAmmo.IsValidRef())
		infiniteAmmo.SetInt(m_savedInfiniteAmmo);
	if (buyGuns.IsValidRef())
		buyGuns.SetInt(m_savedBuyGuns);
	if (buyGrenades.IsValidRef())
		buyGrenades.SetInt(m_savedBuyGrenades);
	if (tGrenades.IsValidRef())
		tGrenades.SetString(m_savedTDefaultGrenades.c_str());
	if (ctGrenades.IsValidRef())
		ctGrenades.SetString(m_savedCTDefaultGrenades.c_str());
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
	m_offsets.freezePeriod = FindSchemaOffset("CCSGameRules", "m_bFreezePeriod");
	m_offsets.roundStartTime = FindSchemaOffset("CCSGameRules", "m_fRoundStartTime");
	m_offsets.restartRoundTime = FindSchemaOffset("CCSGameRules", "m_flRestartRoundTime");

	// Reading GameRules must not depend on unrelated player/loadout fields.
	m_offsets.resolved = m_offsets.gameRules >= 0 &&
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

bool SnowballRoundPlugin::ReadGameRules(int& currentRound, bool& warmup, RoundSnapshot* snapshot)
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
		if (snapshot)
		{
			if (m_offsets.freezePeriod < 0 || m_offsets.roundStartTime < 0 || m_offsets.restartRoundTime < 0)
				return false;
			snapshot->number = currentRound;
			snapshot->warmup = warmup;
			snapshot->freeze = *reinterpret_cast<bool*>(bytes + m_offsets.freezePeriod);
			snapshot->startTime = *reinterpret_cast<float*>(bytes + m_offsets.roundStartTime);
			snapshot->restartTime = *reinterpret_cast<float*>(bytes + m_offsets.restartRoundTime);
		}
		return true;
	}
	return false;
}

void SnowballRoundPlugin::LockWeapons()
{
	if (!RefreshEntitySystem())
		return;
	if (m_primaryAttack < 0 || m_secondaryAttack < 0)
	{
		m_primaryAttack = FindSchemaOffset("CBasePlayerWeapon", "m_nNextPrimaryAttackTick");
		m_secondaryAttack = FindSchemaOffset("CBasePlayerWeapon", "m_nNextSecondaryAttackTick");
		if (m_primaryAttack < 0 || m_secondaryAttack < 0)
		{
			if (!m_weaponSchemaErrorLogged)
				META_CONPRINTF("[SnowballRound] Cannot lock weapons: attack schema fields missing.\n");
			m_weaponSchemaErrorLogged = true;
			return;
		}
	}

	// Execute before simulation, on every frame, so newly bought/picked-up
	// weapons are covered too. Knife and decoy attack timers are untouched.
	for (CEntityIdentity* identity = g_entitySystem->m_EntityList.m_pFirstActiveEntity;
		 identity; identity = identity->m_pNext)
	{
		if (!identity->m_pInstance || !SnowballBlocksWeapon(identity->GetClassname()))
			continue;
		auto* bytes = reinterpret_cast<uint8_t*>(identity->m_pInstance);
		auto& primary = *reinterpret_cast<int*>(bytes + m_primaryAttack);
		auto& secondary = *reinterpret_cast<int*>(bytes + m_secondaryAttack);
		const CEntityHandle handle = identity->GetRefEHandle();
		const auto saved = std::find_if(m_lockedWeapons.begin(), m_lockedWeapons.end(),
			[&](const LockedWeapon& weapon) { return weapon.handle == handle; });
		if (saved == m_lockedWeapons.end())
			m_lockedWeapons.push_back({handle, primary, secondary});
		primary = secondary = std::numeric_limits<int>::max() - 1;
	}
}

void SnowballRoundPlugin::RestoreWeapons()
{
	if (!m_lockedWeapons.empty() && RefreshEntitySystem() &&
		m_primaryAttack >= 0 && m_secondaryAttack >= 0)
	{
		for (const LockedWeapon& weapon : m_lockedWeapons)
		{
			CEntityInstance* entity = ResolveEntity(weapon.handle);
			if (!entity)
				continue;
			auto* bytes = reinterpret_cast<uint8_t*>(entity);
			*reinterpret_cast<int*>(bytes + m_primaryAttack) = weapon.primary;
			*reinterpret_cast<int*>(bytes + m_secondaryAttack) = weapon.secondary;
		}
	}
	m_lockedWeapons.clear();
}

void SnowballRoundPlugin::ApplySnowballLoadoutToAll()
{
	// Give the normal decoy through the engine command path. The companion
	// snowball plugin is responsible for converting it into a snowball.
	if (!g_engine)
		return;

	constexpr int maxClients = 64;
	for (int slot = 0; slot < maxClients; ++slot)
	{
		g_engine->ClientCommand(CPlayerSlot(slot), "give weapon_decoy");
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
