/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "karazhan.h"
#include "AreaTriggerScript.h"
#include "CreatureScript.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedEscortAI.h"
#include "ScriptedGossip.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"

enum Spells
{
    // Barnes
    SPELL_SPOTLIGHT             = 25824,
    SPELL_TUXEDO                = 32616,

    // Berthold
    SPELL_TELEPORT              = 39567,

    // Image of Medivh
    SPELL_FIRE_BALL             = 30967,
    SPELL_UBER_FIREBALL         = 30971,
    SPELL_CONFLAGRATION_BLAST   = 30977,
    SPELL_MANA_SHIELD           = 31635,

    // Wrath of the Titans
    SPELL_WRATH_OF_THE_TITANS   = 30554,

    SPELL_WRATH_PROC_BLAST      = 30605,
    SPELL_WRATH_PROC_BOLT       = 30606,
    SPELL_WRATH_PROC_FLAME      = 30607,
    SPELL_WRATH_PROC_SPITE      = 30608,
    SPELL_WRATH_PROC_CHILL      = 30609,

};

enum Creatures
{
    NPC_ARCANAGOS               = 17652,
    NPC_SPOTLIGHT               = 19525
};

/*######
# npc_barnesAI
######*/

enum Misc
{
    OZ_GOSSIP1_MID = 7421, // I'm not an actor.
    OZ_GOSSIP1_OID = 0,
    OZ_GOSSIP2_MID = 7422, // Ok, I'll give it a try, then.
    OZ_GOSSIP2_OID = 0,
};

enum NPCTexts
{
    BARNES_TEXT_NOT_READY   = 8969,
    BARNES_TEXT_IS_READY    = 8970,
    BARNES_TEXT_IS_READY2   = 8971,
    BARNES_TEXT_WIPED       = 8975
};

#define OZ_GM_GOSSIP1       "[GM] Change event to EVENT_OZ"
#define OZ_GM_GOSSIP2       "[GM] Change event to EVENT_HOOD"
#define OZ_GM_GOSSIP3       "[GM] Change event to EVENT_RAJ"

struct Dialogue
{
    int32 textid;
    uint32 timer;
};

static Dialogue OzDialogue[] =
{
    {0, 6000},
    {1, 18000},
    {2, 9000},
    {3, 15000}
};

static Dialogue HoodDialogue[] =
{
    {4, 6000},
    {5, 10000},
    {6, 14000},
    {7, 15000}
};

static Dialogue RAJDialogue[] =
{
    {8, 5000},
    {9, 7000},
    {10, 14000},
    {11, 14000}
};

// Entries and spawn locations for creatures in Oz event
float Spawns[6][2] =
{
    {17535, -10896},                                        // Dorothee
    {17546, -10891},                                        // Roar
    {17547, -10884},                                        // Tinhead
    {17543, -10902},                                        // Strawman
    {17603, -10892},                                        // Grandmother
    {17534, -10900},                                        // Julianne
};

#define SPAWN_Z             90.5f
#define SPAWN_Y             -1758
#define SPAWN_O             4.738f

class npc_barnes : public CreatureScript
{
public:
    npc_barnes() : CreatureScript("npc_barnes") { }

    struct npc_barnesAI : public npc_escortAI
    {
        npc_barnesAI(Creature* creature) : npc_escortAI(creature)
        {
            m_uiEventId = 0;
            instance = creature->GetInstanceScript();
        }

        InstanceScript* instance;

        ObjectGuid m_uiSpotlightGUID;

        uint32 TalkCount;
        uint32 TalkTimer;
        uint32 m_uiEventId;

        bool PerformanceReady;

        void Reset() override
        {
            m_uiSpotlightGUID.Clear();

            TalkCount = 0;
            TalkTimer = 2000;

            PerformanceReady = false;

            m_uiEventId = instance->GetData(DATA_OPERA_PERFORMANCE);
        }

        void StartEvent()
        {
            instance->SetBossState(DATA_OPERA_PERFORMANCE, IN_PROGRESS);

            //resets count for this event, in case earlier failed
            if (m_uiEventId == EVENT_OZ)
                instance->SetData(DATA_OPERA_OZ_DEATHCOUNT, IN_PROGRESS);

            Start(false, false);
        }

        void JustEngagedWith(Unit* /*who*/) override { }

        void WaypointReached(uint32 waypointId) override
        {
            switch (waypointId)
            {
                case 0:
                    DoCastSelf(SPELL_TUXEDO);
                    instance->HandleGameObject(instance->GetGuidData(DATA_GO_STAGEDOORLEFT), true);
                    break;
                case 4:
                    TalkCount = 0;
                    SetEscortPaused(true);

                    if (Creature* spotlight = me->SummonCreature(NPC_SPOTLIGHT,
                                              me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0.0f,
                                              TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 60000))
                    {
                        spotlight->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                        spotlight->CastSpell(spotlight, SPELL_SPOTLIGHT, false);
                        m_uiSpotlightGUID = spotlight->GetGUID();
                    }
                    break;
                case 8:
                    if (m_uiEventId != EVENT_HOOD) // in red riding hood door should close when gossip with grandma is over
                    {
                        instance->DoUseDoorOrButton(instance->GetGuidData(DATA_GO_STAGEDOORLEFT));
                    }
                    PerformanceReady = true;
                    break;
                case 9:
                    PrepareEncounter();
                    instance->DoUseDoorOrButton(instance->GetGuidData(DATA_GO_CURTAINS));
                    break;
            }
        }

        void Talk(uint32 count)
        {
            int32 text = 0;

            switch (m_uiEventId)
            {
                case EVENT_OZ:
                    if (OzDialogue[count].textid)
                        text = OzDialogue[count].textid;
                    if (OzDialogue[count].timer)
                        TalkTimer = OzDialogue[count].timer;
                    break;

                case EVENT_HOOD:
                    if (HoodDialogue[count].textid)
                        text = HoodDialogue[count].textid;
                    if (HoodDialogue[count].timer)
                        TalkTimer = HoodDialogue[count].timer;
                    break;

                case EVENT_RAJ:
                    if (RAJDialogue[count].textid)
                        text = RAJDialogue[count].textid;
                    if (RAJDialogue[count].timer)
                        TalkTimer = RAJDialogue[count].timer;
                    break;
            }

            if (text)
                CreatureAI::Talk(text);
        }

        void PrepareEncounter()
        {
            LOG_DEBUG("scripts.ai", "Barnes Opera Event - Introduction complete - preparing encounter {}", m_uiEventId);
            uint8 index = 0;
            uint8 count = 0;

            switch (m_uiEventId)
            {
                case EVENT_OZ:
                    index = 0;
                    count = 4;
                    break;
                case EVENT_HOOD:
                    index = 4;
                    count = index + 1;
                    break;
                case EVENT_RAJ:
                    index = 5;
                    count = index + 1;
                    break;
            }

            for (; index < count; ++index)
            {
                uint32 entry = ((uint32)Spawns[index][0]);
                float PosX = Spawns[index][1];

                if (Creature* creature = me->SummonCreature(entry, PosX, SPAWN_Y, SPAWN_Z, SPAWN_O, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, HOUR * 2 * IN_MILLISECONDS))
                    creature->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            }

            instance->SetData(DATA_SPAWN_OPERA_DECORATIONS, m_uiEventId);
        }

        void UpdateAI(uint32 diff) override
        {
            npc_escortAI::UpdateAI(diff);

            if (HasEscortState(STATE_ESCORT_PAUSED))
            {
                if (TalkTimer <= diff)
                {
                    if (TalkCount > 3)
                    {
                        if (Creature* pSpotlight = ObjectAccessor::GetCreature(*me, m_uiSpotlightGUID))
                            pSpotlight->DespawnOrUnsummon();

                        SetEscortPaused(false);
                        return;
                    }

                    Talk(TalkCount);
                    ++TalkCount;
                }
                else TalkTimer -= diff;
            }
        }
    };

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        ClearGossipMenuFor(player);
        npc_barnesAI* pBarnesAI = CAST_AI(npc_barnes::npc_barnesAI, creature->AI());

        switch (action)
        {
            case GOSSIP_ACTION_INFO_DEF+1:
                AddGossipItemFor(player, OZ_GOSSIP2_MID, OZ_GOSSIP2_OID, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
                SendGossipMenuFor(player, BARNES_TEXT_IS_READY2, creature->GetGUID());
                break;
            case GOSSIP_ACTION_INFO_DEF+2:
                CloseGossipMenuFor(player);
                pBarnesAI->StartEvent();
                break;
            case GOSSIP_ACTION_INFO_DEF+3:
                CloseGossipMenuFor(player);
                pBarnesAI->m_uiEventId = EVENT_OZ;
                break;
            case GOSSIP_ACTION_INFO_DEF+4:
                CloseGossipMenuFor(player);
                pBarnesAI->m_uiEventId = EVENT_HOOD;
                break;
            case GOSSIP_ACTION_INFO_DEF+5:
                CloseGossipMenuFor(player);
                pBarnesAI->m_uiEventId = EVENT_RAJ;
                break;
        }

        return true;
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (InstanceScript* instance = creature->GetInstanceScript())
        {
            // Check for death of Moroes and if opera event is not done already
            if (instance->GetBossState(DATA_MOROES) == DONE &&  instance->GetBossState(DATA_OPERA_PERFORMANCE) != DONE)
            {
                AddGossipItemFor(player, OZ_GOSSIP1_MID, OZ_GOSSIP1_OID, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);

                if (player->IsGameMaster())
                {
                    AddGossipItemFor(player, GOSSIP_ICON_DOT, OZ_GM_GOSSIP1, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
                    AddGossipItemFor(player, GOSSIP_ICON_DOT, OZ_GM_GOSSIP2, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 4);
                    AddGossipItemFor(player, GOSSIP_ICON_DOT, OZ_GM_GOSSIP3, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 5);
                }

                if (instance->GetBossState(DATA_OPERA_PERFORMANCE) != FAIL)
                {
                    SendGossipMenuFor(player, BARNES_TEXT_IS_READY, creature->GetGUID());
                }
                else
                {
                    SendGossipMenuFor(player, BARNES_TEXT_WIPED, creature->GetGUID());
                }

                return true;
            }
        }

        SendGossipMenuFor(player, BARNES_TEXT_NOT_READY, creature->GetGUID());
        return true;
    }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetKarazhanAI<npc_barnesAI>(creature);
    }
};

/*###
# npc_image_of_medivh
####*/

enum MedivhTexts
{
    SAY_DIALOG_MEDIVH_1    = 0,
    SAY_DIALOG_ARCANAGOS_2 = 0,
    SAY_DIALOG_MEDIVH_3    = 1,
    SAY_DIALOG_ARCANAGOS_4 = 1,
    SAY_DIALOG_MEDIVH_5    = 2,
    SAY_DIALOG_ARCANAGOS_6 = 2,
    EMOTE_DIALOG_MEDIVH_7  = 3,
    SAY_DIALOG_ARCANAGOS_8 = 3,
    SAY_DIALOG_MEDIVH_9    = 4
};

//static float MedivPos[4] = {-11161.49f, -1902.24f, 91.48f, 1.94f};
static float ArcanagosPos[4] = {-11169.75f, -1881.48f, 107.39f, 4.83f};

class npc_image_of_medivh : public CreatureScript
{
public:
    npc_image_of_medivh() : CreatureScript("npc_image_of_medivh") { }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetKarazhanAI<npc_image_of_medivhAI>(creature);
    }

    struct npc_image_of_medivhAI : public ScriptedAI
    {
        npc_image_of_medivhAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = creature->GetInstanceScript();
            Step = 1;
            YellTimer = 5000;
        }

        InstanceScript* instance;

        ObjectGuid ArcanagosGUID;

        uint32 YellTimer;
        uint8 Step;
        int32 MTimer;
        int32 ATimer;

        bool EventStarted;

        void Reset() override
        {
            ArcanagosGUID.Clear();
            MTimer = 0;
            ATimer = 0;

            if (instance && !instance->GetGuidData(DATA_IMAGE_OF_MEDIVH))
            {
                Creature* Arcanagos = me->SummonCreature(NPC_ARCANAGOS, ArcanagosPos[0], ArcanagosPos[1], ArcanagosPos[2], 0, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                if (!Arcanagos)
                {
                    me->DespawnOrUnsummon();
                    return;
                }

                instance->SetGuidData(DATA_IMAGE_OF_MEDIVH, me->GetGUID());
                EventStarted = true;
                ArcanagosGUID = Arcanagos->GetGUID();

                Arcanagos->SetFacingToObject(me);
                me->SetFacingToObject(Arcanagos);

                Arcanagos->SetCanFly(true);
            }
            else
                me->DespawnOrUnsummon();
        }

        void JustEngagedWith(Unit* /*who*/) override {}

        uint32 NextStep(uint32 nextStep)
        {
            switch (nextStep)
            {
                case 1:
                    Talk(SAY_DIALOG_MEDIVH_1);
                    return 10000;
                case 2:
                    if (Creature* arca = ObjectAccessor::GetCreature((*me), ArcanagosGUID))
                        arca->AI()->Talk(SAY_DIALOG_ARCANAGOS_2);
                    return 20000;
                case 3:
                    Talk(SAY_DIALOG_MEDIVH_3);
                    return 10000;
                case 4:
                    if (Creature* arca = ObjectAccessor::GetCreature((*me), ArcanagosGUID))
                        arca->AI()->Talk(SAY_DIALOG_ARCANAGOS_4);
                    return 20000;
                case 5:
                    Talk(SAY_DIALOG_MEDIVH_5);
                    return 20000;
                case 6:
                    if (Creature* arca = ObjectAccessor::GetCreature((*me), ArcanagosGUID))
                        arca->AI()->Talk(SAY_DIALOG_ARCANAGOS_6);

                    ATimer = 5500;
                    MTimer = 6600;
                    return 10000;
                case 7:
                    return 1000;
                case 8:
                    me->CastSpell(me, SPELL_MANA_SHIELD, true);
                    return 5500;
                case 9:
                    me->TextEmote(EMOTE_DIALOG_MEDIVH_7);
                    me->CastSpell(me, 30972, true);
                    return 10000;
                case 10:
                    me->RemoveAurasDueToSpell(30972);
                    if (Creature* arca = ObjectAccessor::GetCreature((*me), ArcanagosGUID))
                        me->CastSpell(arca, SPELL_CONFLAGRATION_BLAST, false);
                    return 1000;
                case 11:
                    if (Creature* arca = ObjectAccessor::GetCreature((*me), ArcanagosGUID))
                        arca->AI()->Talk(SAY_DIALOG_ARCANAGOS_8);
                    return 5000;
                case 12:
                    if (Creature* arca = ObjectAccessor::GetCreature((*me), ArcanagosGUID))
                    {
                        arca->SetSpeed(MOVE_RUN, 2.0f);
                        arca->GetMotionMaster()->MovePoint(0, -11010.82f, -1761.18f, 156.47f);
                        arca->InterruptNonMeleeSpells(true);
                    }
                    return 10000;
                case 13:
                    Talk(SAY_DIALOG_MEDIVH_9);
                    return 10000;
                case 14:
                    if (me->GetMap()->IsDungeon())
                    {
                        InstanceMap::PlayerList const& PlayerList = me->GetMap()->GetPlayers();
                        for (InstanceMap::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
                        {
                            if (i->GetSource()->GetQuestStatus(9645) == QUEST_STATUS_INCOMPLETE)
                            {
                                i->GetSource()->GroupEventHappens(9645, me);
                                break;
                            }
                        }
                    }

                    me->DespawnOrUnsummon(100);
                    if (Creature* arca = ObjectAccessor::GetCreature((*me), ArcanagosGUID))
                        arca->DespawnOrUnsummon(100);

                    return 5000;
                default:
                    return 2000;
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (YellTimer <= diff)
            {
                if (EventStarted)
                    YellTimer = NextStep(Step++);
            }
            else YellTimer -= diff;

            if (Step >= 7 && Step <= 8)
            {
                ATimer += diff;
                MTimer += diff;
                if (ATimer >= 6000)
                {
                    if (Unit* arca = ObjectAccessor::GetUnit((*me), ArcanagosGUID))
                        arca->CastSpell(me, SPELL_FIRE_BALL, false);
                    ATimer = 0;
                }
                if (MTimer >= 6000)
                {
                    if (Unit* arca = ObjectAccessor::GetUnit((*me), ArcanagosGUID))
                        me->CastSpell(arca, SPELL_FIRE_BALL, false);
                    MTimer = 0;
                }
            }
        }
    };
};

class at_karazhan_side_entrance : public OnlyOnceAreaTriggerScript
{
public:
    at_karazhan_side_entrance() : OnlyOnceAreaTriggerScript("at_karazhan_side_entrance") { }

    bool _OnTrigger(Player* player, AreaTrigger const* /*at*/) override
    {
        if (InstanceScript* instance = player->GetInstanceScript())
        {
            if (instance->GetBossState(DATA_OPERA_PERFORMANCE) == DONE)
            {
                if (GameObject* door = instance->GetGameObject(DATA_GO_SIDE_ENTRANCE_DOOR))
                {
                    instance->HandleGameObject(ObjectGuid::Empty, true, door);
                    door->RemoveGameObjectFlag(GO_FLAG_LOCKED);
                }
            }
        }

        return false;
    }
};

class spell_karazhan_temptation : public AuraScript
{
    PrepareAuraScript(spell_karazhan_temptation);

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        if (eventInfo.GetActionTarget())
        {
            GetTarget()->CastSpell(eventInfo.GetActionTarget(), GetSpellInfo()->Effects[aurEff->GetEffIndex()].TriggerSpell, true);
        }
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_karazhan_temptation::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 30610 - Wrath of the Titans Stacker
class spell_karazhan_wrath_titans_stacker : public SpellScript
{
    PrepareSpellScript(spell_karazhan_wrath_titans_stacker);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WRATH_OF_THE_TITANS });
    }

    void HandleDummy(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        Unit* caster = GetCaster();
        if (!caster)
            return;

        caster->CastSpell(caster, SPELL_WRATH_OF_THE_TITANS, true);
        if (Aura* aur = caster->GetAura(SPELL_WRATH_OF_THE_TITANS))
            aur->SetStackAmount(5);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_karazhan_wrath_titans_stacker::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 30554 - Wrath of the Titans
class spell_karazhan_wrath_titans_aura : public AuraScript
{
    PrepareAuraScript(spell_karazhan_wrath_titans_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WRATH_PROC_BLAST, SPELL_WRATH_PROC_BOLT, SPELL_WRATH_PROC_FLAME, SPELL_WRATH_PROC_SPITE, SPELL_WRATH_PROC_CHILL });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (!eventInfo.GetSpellInfo())
            return false;

        if (GetFirstSchoolInMask(eventInfo.GetSpellInfo()->GetSchoolMask()) == SPELL_SCHOOL_NORMAL)
            return false;

        if (GetFirstSchoolInMask(eventInfo.GetSpellInfo()->GetSchoolMask()) == SPELL_SCHOOL_HOLY)
            return false;

        return true;
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* target = eventInfo.GetActionTarget();
        Player* caster = GetTarget()->ToPlayer();
        if (!target || !caster)
            return;

        uint32 spellId = 0;
        switch (GetFirstSchoolInMask(eventInfo.GetSpellInfo()->GetSchoolMask()))
        {
            case SPELL_SCHOOL_FIRE:
                spellId = SPELL_WRATH_PROC_FLAME;
                break;
            case SPELL_SCHOOL_NATURE:
                spellId = SPELL_WRATH_PROC_BOLT;
                break;
            case SPELL_SCHOOL_FROST:
                spellId = SPELL_WRATH_PROC_CHILL;
                break;
            case SPELL_SCHOOL_SHADOW:
                spellId = SPELL_WRATH_PROC_SPITE;
                break;
            case SPELL_SCHOOL_ARCANE:
                spellId = SPELL_WRATH_PROC_BLAST;
                break;
            default:
                return;
        }

        caster->CastSpell(target, spellId, true);
        ModStackAmount(-1);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_karazhan_wrath_titans_aura::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_karazhan_wrath_titans_aura::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

void AddSC_karazhan()
{
    new npc_barnes();
    new npc_image_of_medivh();
    new at_karazhan_side_entrance();
    RegisterSpellScript(spell_karazhan_temptation);
    RegisterSpellScript(spell_karazhan_wrath_titans_stacker);
    RegisterSpellScript(spell_karazhan_wrath_titans_aura);
}
