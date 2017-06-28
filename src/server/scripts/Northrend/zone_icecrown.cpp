/*
 * Copyright (C) 2008-2014 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2006-2009 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellAuras.h"
#include "Player.h"
#include "TemporarySummon.h"
#include "CombatAI.h"

/*######
## npc_squire_david
######*/

enum SquireDavid
{
    QUEST_THE_ASPIRANT_S_CHALLENGE_H                    = 13680,
    QUEST_THE_ASPIRANT_S_CHALLENGE_A                    = 13679,

    NPC_ARGENT_VALIANT                                  = 33448,

    GOSSIP_TEXTID_SQUIRE                                = 14407
};

#define GOSSIP_SQUIRE_ITEM_1 "I am ready to fight!"
#define GOSSIP_SQUIRE_ITEM_2 "How do the Argent Crusader raiders fight?"

class npc_squire_david : public CreatureScript
{
public:
    npc_squire_david() : CreatureScript("npc_squire_david") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (player->GetQuestStatus(QUEST_THE_ASPIRANT_S_CHALLENGE_H) == QUEST_STATUS_INCOMPLETE ||
            player->GetQuestStatus(QUEST_THE_ASPIRANT_S_CHALLENGE_A) == QUEST_STATUS_INCOMPLETE)//We need more info about it.
        {
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SQUIRE_ITEM_1, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+1);
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SQUIRE_ITEM_2, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+2);
        }

        player->SEND_GOSSIP_MENU(GOSSIP_TEXTID_SQUIRE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        player->PlayerTalkClass->ClearMenus();
        if (action == GOSSIP_ACTION_INFO_DEF+1)
        {
            player->CLOSE_GOSSIP_MENU();
            creature->SummonCreature(NPC_ARGENT_VALIANT, 8575.451f, 952.472f, 547.554f, 0.38f);
        }
        return true;
    }
};

/*######
## npc_argent_valiant
######*/

enum ArgentValiant
{
    SPELL_CHARGE                = 63010,
    SPELL_SHIELD_BREAKER        = 65147,
    SPELL_KILL_CREDIT           = 63049
};

class npc_argent_valiant : public CreatureScript
{
public:
    npc_argent_valiant() : CreatureScript("npc_argent_valiant") { }

    struct npc_argent_valiantAI : public ScriptedAI
    {
        npc_argent_valiantAI(Creature* creature) : ScriptedAI(creature)
        {
            creature->GetMotionMaster()->MovePoint(0, 8599.258f, 963.951f, 547.553f);
            creature->setFaction(35); //wrong faction in db?
        }

        uint32 uiChargeTimer;
        uint32 uiShieldBreakerTimer;

        void Reset() override
        {
            uiChargeTimer = 7000;
            uiShieldBreakerTimer = 10000;
        }

        void MovementInform(uint32 uiType, uint32 /*uiId*/) override
        {
            if (uiType != POINT_MOTION_TYPE)
                return;

            me->setFaction(14);
        }

        void DamageTaken(Unit* pDoneBy, uint32& uiDamage) override
        {
            if (uiDamage > me->GetHealth() && pDoneBy->GetTypeId() == TYPEID_PLAYER)
            {
                uiDamage = 0;
                pDoneBy->CastSpell(pDoneBy, SPELL_KILL_CREDIT, true);
                me->setFaction(35);
                me->DespawnOrUnsummon(5000);
                me->SetHomePosition(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), me->GetOrientation());
                EnterEvadeMode();
            }
        }

        void UpdateAI(uint32 uiDiff) override
        {
            if (!UpdateVictim())
                return;

            if (uiChargeTimer <= uiDiff)
            {
                DoCastVictim(SPELL_CHARGE);
                uiChargeTimer = 7000;
            } else uiChargeTimer -= uiDiff;

            if (uiShieldBreakerTimer <= uiDiff)
            {
                DoCastVictim(SPELL_SHIELD_BREAKER);
                uiShieldBreakerTimer = 10000;
            } else uiShieldBreakerTimer -= uiDiff;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_argent_valiantAI(creature);
    }
};

/*######
## npc_guardian_pavilion
######*/

enum GuardianPavilion
{
    SPELL_TRESPASSER_H                            = 63987,
    AREA_SUNREAVER_PAVILION                       = 4676,

    AREA_SILVER_COVENANT_PAVILION                 = 4677,
    SPELL_TRESPASSER_A                            = 63986,
};

class npc_guardian_pavilion : public CreatureScript
{
public:
    npc_guardian_pavilion() : CreatureScript("npc_guardian_pavilion") { }

    struct npc_guardian_pavilionAI : public ScriptedAI
    {
        npc_guardian_pavilionAI(Creature* creature) : ScriptedAI(creature)
        {
            SetCombatMovement(false);
        }

        void MoveInLineOfSight(Unit* who) override

        {
            if (me->GetAreaId() != AREA_SUNREAVER_PAVILION && me->GetAreaId() != AREA_SILVER_COVENANT_PAVILION)
                return;

            if (!who || who->GetTypeId() != TYPEID_PLAYER || !me->IsHostileTo(who) || !me->isInBackInMap(who, 5.0f))
                return;

            if (who->HasAura(SPELL_TRESPASSER_H) || who->HasAura(SPELL_TRESPASSER_A))
                return;

            if (who->ToPlayer()->GetTeamId() == TEAM_ALLIANCE)
                who->CastSpell(who, SPELL_TRESPASSER_H, true);
            else
                who->CastSpell(who, SPELL_TRESPASSER_A, true);

        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_guardian_pavilionAI(creature);
    }
};

/*######
## npc_vereth_the_cunning
######*/

enum VerethTheCunning
{
    NPC_GEIST_RETURN_BUNNY_KC   = 31049,
    NPC_LITHE_STALKER           = 30894,
    SPELL_SUBDUED_LITHE_STALKER = 58151,
};

class npc_vereth_the_cunning : public CreatureScript
{
public:
    npc_vereth_the_cunning() : CreatureScript("npc_vereth_the_cunning") { }

    struct npc_vereth_the_cunningAI : public ScriptedAI
    {
        npc_vereth_the_cunningAI(Creature* creature) : ScriptedAI(creature) { }

        void MoveInLineOfSight(Unit* who) override

        {
            ScriptedAI::MoveInLineOfSight(who);

            if (who->GetEntry() == NPC_LITHE_STALKER && me->IsWithinDistInMap(who, 10.0f))
            {
                if (Unit* owner = who->GetCharmer())
                {
                    if (who->HasAura(SPELL_SUBDUED_LITHE_STALKER))
                        {
                            owner->ToPlayer()->KilledMonsterCredit(NPC_GEIST_RETURN_BUNNY_KC, 0);
                            who->ToCreature()->DisappearAndDie();

                    }
                }
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_vereth_the_cunningAI(creature);
    }
};

/*######
* npc_tournament_training_dummy
######*/
enum TournamentDummy
{
    NPC_CHARGE_TARGET         = 33272,
    NPC_MELEE_TARGET          = 33229,
    NPC_RANGED_TARGET         = 33243,

    SPELL_CHARGE_CREDIT       = 62658,
    SPELL_MELEE_CREDIT        = 62672,
    SPELL_RANGED_CREDIT       = 62673,

    SPELL_PLAYER_THRUST       = 62544,
    SPELL_PLAYER_BREAK_SHIELD = 62626,
    SPELL_PLAYER_CHARGE       = 62874,

    SPELL_RANGED_DEFEND       = 62719,
    SPELL_CHARGE_DEFEND       = 64100,
    SPELL_VULNERABLE          = 62665,

    SPELL_COUNTERATTACK       = 62709,

    EVENT_DUMMY_RECAST_DEFEND = 1,
    EVENT_DUMMY_RESET         = 2,
};

class npc_tournament_training_dummy : public CreatureScript
{
    public:
        npc_tournament_training_dummy(): CreatureScript("npc_tournament_training_dummy"){ }

        struct npc_tournament_training_dummyAI : ScriptedAI
        {
            npc_tournament_training_dummyAI(Creature* creature) : ScriptedAI(creature)
            {
                SetCombatMovement(false);
            }

            EventMap events;
            bool isVulnerable;

            void Reset() override
            {
                me->SetControlled(true, UNIT_STATE_STUNNED);
                me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
                isVulnerable = false;

                // Cast Defend spells to max stack size
                switch (me->GetEntry())
                {
                    case NPC_CHARGE_TARGET:
                        DoCast(SPELL_CHARGE_DEFEND);
                        break;
                    case NPC_RANGED_TARGET:
                        me->CastCustomSpell(SPELL_RANGED_DEFEND, SPELLVALUE_AURA_STACK, 3, me);
                        break;
                }

                events.Reset();
                events.ScheduleEvent(EVENT_DUMMY_RECAST_DEFEND, 5000);
            }

            void EnterEvadeMode() override
            {
                if (!_EnterEvadeMode())
                    return;

                Reset();
            }

            void DamageTaken(Unit* /*attacker*/, uint32& damage) override
            {
                damage = 0;
                events.RescheduleEvent(EVENT_DUMMY_RESET, 10000);
            }

            void SpellHit(Unit* caster, SpellInfo const* spell) override
            {
                switch (me->GetEntry())
                {
                    case NPC_CHARGE_TARGET:
                        if (spell->Id == SPELL_PLAYER_CHARGE)
                            if (isVulnerable)
                                DoCast(caster, SPELL_CHARGE_CREDIT, true);
                        break;
                    case NPC_MELEE_TARGET:
                        if (spell->Id == SPELL_PLAYER_THRUST)
                        {
                            DoCast(caster, SPELL_MELEE_CREDIT, true);

                            if (Unit* target = caster->GetVehicleBase())
                                DoCast(target, SPELL_COUNTERATTACK, true);
                        }
                        break;
                    case NPC_RANGED_TARGET:
                        if (spell->Id == SPELL_PLAYER_BREAK_SHIELD)
                            if (isVulnerable)
                                DoCast(caster, SPELL_RANGED_CREDIT, true);
                        break;
                }

                if (spell->Id == SPELL_PLAYER_BREAK_SHIELD)
                    if (!me->HasAura(SPELL_CHARGE_DEFEND) && !me->HasAura(SPELL_RANGED_DEFEND))
                        isVulnerable = true;
            }

            void UpdateAI(uint32 diff) override
            {
                events.Update(diff);

                switch (events.ExecuteEvent())
                {
                    case EVENT_DUMMY_RECAST_DEFEND:
                        switch (me->GetEntry())
                        {
                            case NPC_CHARGE_TARGET:
                            {
                                if (!me->HasAura(SPELL_CHARGE_DEFEND))
                                    DoCast(SPELL_CHARGE_DEFEND);
                                break;
                            }
                            case NPC_RANGED_TARGET:
                            {
                                Aura* defend = me->GetAura(SPELL_RANGED_DEFEND);
                                if (!defend || defend->GetStackAmount() < 3 || defend->GetDuration() <= 8000)
                                    DoCast(SPELL_RANGED_DEFEND);
                                break;
                            }
                        }
                        isVulnerable = false;
                        events.ScheduleEvent(EVENT_DUMMY_RECAST_DEFEND, 5000);
                        break;
                    case EVENT_DUMMY_RESET:
                        if (UpdateVictim())
                        {
                            EnterEvadeMode();
                            events.ScheduleEvent(EVENT_DUMMY_RESET, 10000);
                        }
                        break;
                }

                if (!UpdateVictim())
                    return;

                if (!me->HasUnitState(UNIT_STATE_STUNNED))
                    me->SetControlled(true, UNIT_STATE_STUNNED);
            }

            void MoveInLineOfSight(Unit* /*who*/) override { }

        };

        CreatureAI* GetAI(Creature* creature) const override
        {
            return new npc_tournament_training_dummyAI(creature);
        }

};

// Battle for Crusaders' Pinnacle
enum BlessedBanner
{
    SPELL_BLESSING_OF_THE_CRUSADE       = 58026,
    SPELL_THREAT_PULSE                  = 58113,
    SPELL_CRUSADERS_SPIRE_VICTORY       = 58084,
    SPELL_TORCH                         = 58121,

    NPC_BLESSED_BANNER                  = 30891,
    NPC_CRUSADER_LORD_DALFORS           = 31003,
    NPC_ARGENT_BATTLE_PRIEST            = 30919,
    NPC_ARGENT_MASON                    = 30900,
    NPC_REANIMATED_CAPTAIN              = 30986,
    NPC_SCOURGE_DRUDGE                  = 30984,
    NPC_HIDEOUS_PLAGEBRINGER            = 30987,
    NPC_HALOF_THE_DEATHBRINGER          = 30989,
    NPC_LK                              = 31013,

    BANNER_SAY                          = 0, // "The Blessed Banner of the Crusade has been planted.\n Defend the banner from all attackers!"
    DALFORS_SAY_PRE_1                   = 0, // "BY THE LIGHT! Those damned monsters! Look at what they've done to our people!"
    DALFORS_SAY_PRE_2                   = 1, // "Burn it down, boys. Burn it all down."
    DALFORS_SAY_START                   = 2, // "Let 'em come. They'll pay for what they've done!"
    DALFORS_YELL_FINISHED               = 3, // "We've done it, lads! We've taken the pinnacle from the Scourge! Report to Father Gustav at once and tell him the good news! We're gonna get to buildin' and settin' up! Go!"
    LK_TALK_1                           = 0, // "Leave no survivors!"
    LK_TALK_2                           = 1, // "Cower before my terrible creations!"
    LK_TALK_3                           = 2, // "Feast my children! Feast upon the flesh of the living!"
    LK_TALK_4                           = 3, // "Lay down your arms and surrender your souls!"

    EVENT_SPAWN                         = 1,
    EVENT_INTRO_1                       = 2,
    EVENT_INTRO_2                       = 3,
    EVENT_INTRO_3                       = 4,
    EVENT_MASON_ACTION                  = 5,
    EVENT_START_FIGHT                   = 6,
    EVENT_WAVE_SPAWN                    = 7,
    EVENT_HALOF                         = 8,
    EVENT_ENDED                         = 9,
};

Position const DalforsPos[3] =
{
    {6458.703f, 403.858f, 490.498f, 3.1205f}, // Dalfors spawn point
    {6422.950f, 423.335f, 510.451f, 0.0f}, // Dalfors intro pos
    {6426.343f, 420.515f, 508.650f, 0.0f}, // Dalfors fight pos
};

Position const Priest1Pos[2] =
{
    {6462.025f, 403.681f, 489.721f, 3.1007f}, // priest1 spawn point
    {6421.480f, 423.576f, 510.781f, 5.7421f}, // priest1 intro pos
};

Position const Priest2Pos[2] =
{
    {6463.969f, 407.198f, 489.240f, 2.2689f}, // priest2 spawn point
    {6419.778f, 421.404f, 510.972f, 5.7421f}, // priest2 intro pos
};

Position const Priest3Pos[2] =
{
    {6464.371f, 400.944f, 489.186f, 6.1610f}, // priest3 spawn point
    {6423.516f, 425.782f, 510.774f, 5.7421f}, // priest3 intro pos
};

Position const Mason1Pos[3] =
{
    {6462.929f, 409.826f, 489.392f, 3.0968f}, // mason1 spawn point
    {6428.163f, 421.960f, 508.297f, 0.0f}, // mason1 intro pos
    {6414.335f, 454.904f, 511.395f, 2.8972f}, // mason1 action pos
};

Position const Mason2Pos[3] =
{
    {6462.650f, 405.670f, 489.576f, 2.9414f}, // mason2 spawn point
    {6426.250f, 419.194f, 508.219f, 0.0f}, // mason2 intro pos
    {6415.014f, 446.849f, 511.395f, 3.1241f}, // mason2 action pos
};

Position const Mason3Pos[3] =
{
    {6462.646f, 401.218f, 489.601f, 2.7864f}, // mason3 spawn point
    {6423.855f, 416.598f, 508.305f, 0.0f}, // mason3 intro pos
    {6417.070f, 438.824f, 511.395f, 3.6651f}, // mason3 action pos
};

class npc_blessed_banner : public CreatureScript
{
public:
    npc_blessed_banner() : CreatureScript("npc_blessed_banner") { }

    struct npc_blessed_bannerAI : public ScriptedAI
    {
        npc_blessed_bannerAI(Creature* creature) : ScriptedAI(creature), Summons(me)
        {
            HalofSpawned = false;
            PhaseCount = 0;
            Summons.DespawnAll();

            SetCombatMovement(false);
        }

        EventMap events;

        bool HalofSpawned;

        uint32 PhaseCount;

        SummonList Summons;

        uint64 guidDalfors;
        uint64 guidPriest[3];
        uint64 guidMason[3];
        uint64 guidHalof;

        void Reset() override
        {
            me->setRegeneratingHealth(false);
            DoCast(SPELL_THREAT_PULSE);
            Talk(BANNER_SAY);
            events.ScheduleEvent(EVENT_SPAWN, 3000);
        }

        void EnterCombat(Unit* /*who*/) override { }

        void MoveInLineOfSight(Unit* /*who*/) override { }


        void JustSummoned(Creature* Summoned) override
        {
            Summons.Summon(Summoned);
        }

        void JustDied(Unit* /*killer*/) override
        {
            Summons.DespawnAll();
            me->DespawnOrUnsummon();
        }

        void UpdateAI(uint32 diff) override
        {
            events.Update(diff);

            switch (events.ExecuteEvent())
            {
                case EVENT_SPAWN:
                    {
                        if (Creature* Dalfors = DoSummon(NPC_CRUSADER_LORD_DALFORS, DalforsPos[0]))
                        {
                            guidDalfors = Dalfors->GetGUID();
                            Dalfors->GetMotionMaster()->MovePoint(0, DalforsPos[1]);
                        }
                        if (Creature* Priest1 = DoSummon(NPC_ARGENT_BATTLE_PRIEST, Priest1Pos[0]))
                        {
                            guidPriest[0] = Priest1->GetGUID();
                            Priest1->GetMotionMaster()->MovePoint(0, Priest1Pos[1]);
                        }
                        if (Creature* Priest2 = DoSummon(NPC_ARGENT_BATTLE_PRIEST, Priest2Pos[0]))
                        {
                            guidPriest[1] = Priest2->GetGUID();
                            Priest2->GetMotionMaster()->MovePoint(0, Priest2Pos[1]);
                        }
                        if (Creature* Priest3 = DoSummon(NPC_ARGENT_BATTLE_PRIEST, Priest3Pos[0]))
                        {
                            guidPriest[2] = Priest3->GetGUID();
                            Priest3->GetMotionMaster()->MovePoint(0, Priest3Pos[1]);
                        }
                        if (Creature* Mason1 = DoSummon(NPC_ARGENT_MASON, Mason1Pos[0]))
                        {
                            guidMason[0] = Mason1->GetGUID();
                            Mason1->GetMotionMaster()->MovePoint(0, Mason1Pos[1]);
                        }
                        if (Creature* Mason2 = DoSummon(NPC_ARGENT_MASON, Mason2Pos[0]))
                        {
                            guidMason[1] = Mason2->GetGUID();
                            Mason2->GetMotionMaster()->MovePoint(0, Mason2Pos[1]);
                        }
                        if (Creature* Mason3 = DoSummon(NPC_ARGENT_MASON, Mason3Pos[0]))
                        {
                            guidMason[2] = Mason3->GetGUID();
                            Mason3->GetMotionMaster()->MovePoint(0, Mason3Pos[1]);
                        }
                        events.ScheduleEvent(EVENT_INTRO_1, 15000);
                    }
                    break;
                case EVENT_INTRO_1:
                    {
                        if (Creature* Dalfors = ObjectAccessor::GetCreature(*me, guidDalfors))
                            Dalfors->AI()->Talk(DALFORS_SAY_PRE_1);
                        events.ScheduleEvent(EVENT_INTRO_2, 5000);
                    }
                    break;
                case EVENT_INTRO_2:
                    {
                        if (Creature* Dalfors = ObjectAccessor::GetCreature(*me, guidDalfors))
                        {
                            Dalfors->SetFacingTo(6.215f);
                            Dalfors->AI()->Talk(DALFORS_SAY_PRE_2);
                        }
                    events.ScheduleEvent(EVENT_INTRO_3, 5000);
                    }
                    break;
                case EVENT_INTRO_3:
                    {
                        if (Creature* Dalfors = ObjectAccessor::GetCreature(*me, guidDalfors))
                        {
                            Dalfors->GetMotionMaster()->MovePoint(0, DalforsPos[2]);
                            Dalfors->SetHomePosition(DalforsPos[2]);
                        }
                        if (Creature* Priest1 = ObjectAccessor::GetCreature(*me, guidPriest[0]))
                        {
                            Priest1->SetFacingTo(5.7421f);
                            Priest1->SetHomePosition(Priest1Pos[1]);
                        }
                        if (Creature* Priest2 = ObjectAccessor::GetCreature(*me, guidPriest[1]))
                        {
                            Priest2->SetFacingTo(5.7421f);
                            Priest2->SetHomePosition(Priest2Pos[1]);
                        }
                        if (Creature* Priest3 = ObjectAccessor::GetCreature(*me, guidPriest[2]))
                        {
                            Priest3->SetFacingTo(5.7421f);
                            Priest3->SetHomePosition(Priest3Pos[1]);
                        }
                        if (Creature* Mason1 = ObjectAccessor::GetCreature(*me, guidMason[0]))
                        {
                            Mason1->GetMotionMaster()->MovePoint(0, Mason1Pos[2]);
                            Mason1->SetHomePosition(Mason1Pos[2]);
                        }
                        if (Creature* Mason2 = ObjectAccessor::GetCreature(*me, guidMason[1]))
                        {
                            Mason2->GetMotionMaster()->MovePoint(0, Mason2Pos[2]);
                            Mason2->SetHomePosition(Mason2Pos[2]);
                        }
                        if (Creature* Mason3 = ObjectAccessor::GetCreature(*me, guidMason[2]))
                        {
                            Mason3->GetMotionMaster()->MovePoint(0, Mason3Pos[2]);
                            Mason3->SetHomePosition(Mason3Pos[2]);
                        }
                        events.ScheduleEvent(EVENT_START_FIGHT, 5000);
                        events.ScheduleEvent(EVENT_MASON_ACTION, 15000);
                    }
                    break;
                case EVENT_MASON_ACTION:
                    {
                        if (Creature* Mason1 = ObjectAccessor::GetCreature(*me, guidMason[0]))
                        {
                            Mason1->SetFacingTo(2.8972f);
                            Mason1->AI()->SetData(1, 1); // triggers SAI actions on npc
                        }
                        if (Creature* Mason2 = ObjectAccessor::GetCreature(*me, guidMason[1]))
                        {
                            Mason2->SetFacingTo(3.1241f);
                            Mason2->AI()->SetData(1, 1); // triggers SAI actions on npc
                        }
                        if (Creature* Mason3 = ObjectAccessor::GetCreature(*me, guidMason[2]))
                        {
                            Mason3->SetFacingTo(3.6651f);
                            Mason3->AI()->SetData(1, 1); // triggers SAI actions on npc
                        }
                    }
                    break;
                case EVENT_START_FIGHT:
                    {
                        if (Creature* LK = GetClosestCreatureWithEntry(me, NPC_LK, 100))
                            LK->AI()->Talk(LK_TALK_1);
                        if (Creature* Dalfors = ObjectAccessor::GetCreature(*me, guidDalfors))
                            Dalfors->AI()->Talk(DALFORS_SAY_START);
                        events.ScheduleEvent(EVENT_WAVE_SPAWN, 1000);
                    }
                    break;
                case EVENT_WAVE_SPAWN:
                    {
                        if (PhaseCount == 3)
                        {
                            if (Creature* LK = GetClosestCreatureWithEntry(me, NPC_LK, 100))
                                LK->AI()->Talk(LK_TALK_2);
                        }
                        else if (PhaseCount == 6)
                        {
                            if (Creature* LK = GetClosestCreatureWithEntry(me, NPC_LK, 100))
                                LK->AI()->Talk(LK_TALK_3);
                        }
                        if (Creature* tempsum = DoSummon(NPC_SCOURGE_DRUDGE, Mason3Pos[0]))
                            {
                                tempsum->SetHomePosition(DalforsPos[2]);
                                tempsum->AI()->AttackStart(GetClosestCreatureWithEntry(me, NPC_BLESSED_BANNER, 100));
                            }
                        if (urand(0, 1) == 0)
                        {
                            if (Creature* tempsum = DoSummon(NPC_HIDEOUS_PLAGEBRINGER, Mason1Pos[0]))
                            {
                                tempsum->SetHomePosition(DalforsPos[2]);
                                tempsum->AI()->AttackStart(GetClosestCreatureWithEntry(me, NPC_BLESSED_BANNER, 100));
                            }
                            if (Creature* tempsum = DoSummon(NPC_HIDEOUS_PLAGEBRINGER, Mason2Pos[0]))
                            {
                                tempsum->SetHomePosition(DalforsPos[2]);
                                tempsum->AI()->AttackStart(GetClosestCreatureWithEntry(me, NPC_BLESSED_BANNER, 100));
                            }
                        }
                        else
                        {
                            if (Creature* tempsum = DoSummon(NPC_REANIMATED_CAPTAIN, Mason1Pos[0]))
                            {
                                tempsum->SetHomePosition(DalforsPos[2]);
                                tempsum->AI()->AttackStart(GetClosestCreatureWithEntry(me, NPC_BLESSED_BANNER, 100));
                            }
                            if (Creature* tempsum = DoSummon(NPC_REANIMATED_CAPTAIN, Mason2Pos[0]))
                            {
                                tempsum->SetHomePosition(DalforsPos[2]);
                                tempsum->AI()->AttackStart(GetClosestCreatureWithEntry(me, NPC_BLESSED_BANNER, 100));
                            }
                        }

                        PhaseCount++;

                        if (PhaseCount < 8)
                            events.ScheduleEvent(EVENT_WAVE_SPAWN, urand(10000, 20000));
                        else
                            events.ScheduleEvent(EVENT_HALOF, urand(10000, 20000));
                    }
                    break;
                case EVENT_HALOF:
                    {
                        if (Creature* LK = GetClosestCreatureWithEntry(me, NPC_LK, 100))
                            LK->AI()->Talk(LK_TALK_4);
                        if (Creature* tempsum = DoSummon(NPC_SCOURGE_DRUDGE, Mason1Pos[0]))
                        {
                            tempsum->SetHomePosition(DalforsPos[2]);
                            tempsum->AI()->AttackStart(GetClosestCreatureWithEntry(me, NPC_BLESSED_BANNER, 100));
                        }
                        if (Creature* tempsum = DoSummon(NPC_SCOURGE_DRUDGE, Mason2Pos[0]))
                        {
                            tempsum->SetHomePosition(DalforsPos[2]);
                            tempsum->AI()->AttackStart(GetClosestCreatureWithEntry(me, NPC_BLESSED_BANNER, 100));
                        }
                        if (Creature* tempsum = DoSummon(NPC_HALOF_THE_DEATHBRINGER, DalforsPos[0]))
                        {
                            HalofSpawned = true;
                            guidHalof = tempsum->GetGUID();
                            tempsum->SetHomePosition(DalforsPos[2]);
                            tempsum->AI()->AttackStart(GetClosestCreatureWithEntry(me, NPC_BLESSED_BANNER, 100));
                        }
                    }
                    break;
                case EVENT_ENDED:
                    {
                        Summons.DespawnAll();
                        me->DespawnOrUnsummon();
                    }
                    break;
            }

            if (PhaseCount == 8)
                if (Creature* Halof = ObjectAccessor::GetCreature(*me, guidHalof))
                    if (Halof->isDead())
                    {
                        DoCast(me, SPELL_CRUSADERS_SPIRE_VICTORY, true);
                        Summons.DespawnEntry(NPC_HIDEOUS_PLAGEBRINGER);
                        Summons.DespawnEntry(NPC_REANIMATED_CAPTAIN);
                        Summons.DespawnEntry(NPC_SCOURGE_DRUDGE);
                        Summons.DespawnEntry(NPC_HALOF_THE_DEATHBRINGER);
                        if (Creature* Dalfors = ObjectAccessor::GetCreature(*me, guidDalfors))
                            Dalfors->AI()->Talk(DALFORS_YELL_FINISHED);
                        events.ScheduleEvent(EVENT_ENDED, 10000);
                    }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_blessed_bannerAI(creature);
    }
};

/*######
## Borrowed Technology - Id: 13291, The Solution Solution (daily) - Id: 13292, Volatility - Id: 13239, Volatiliy - Id: 13261 (daily)
######*/

enum BorrowedTechnologyAndVolatility
{
    // Spells
    SPELL_GRAB             = 59318,
    SPELL_PING_BUNNY       = 59375,
    SPELL_IMMOLATION       = 54690,
    SPELL_EXPLOSION        = 59335,
    SPELL_RIDE             = 59319,

    // Points
    POINT_GRAB_DECOY       = 1,
    POINT_FLY_AWAY         = 2,

    // Events
    EVENT_FLY_AWAY         = 1
};

class npc_frostbrood_skytalon : public CreatureScript
{
    public:
        npc_frostbrood_skytalon() : CreatureScript("npc_frostbrood_skytalon") { }

        struct npc_frostbrood_skytalonAI : public VehicleAI
        {
            npc_frostbrood_skytalonAI(Creature* creature) : VehicleAI(creature) { }

            EventMap events;

            void IsSummonedBy(Unit* summoner) override
            {
                me->GetMotionMaster()->MovePoint(POINT_GRAB_DECOY, summoner->GetPositionX(), summoner->GetPositionY(), summoner->GetPositionZ());
            }

            void MovementInform(uint32 type, uint32 id) override
            {
                if (type != POINT_MOTION_TYPE)
                    return;

                if (id == POINT_GRAB_DECOY)
                    if (TempSummon* summon = me->ToTempSummon())
                        if (Unit* summoner = summon->GetSummoner())
                            DoCast(summoner, SPELL_GRAB);
            }

            void UpdateAI(uint32 diff) override
            {
                VehicleAI::UpdateAI(diff);
                events.Update(diff);

                while (uint32 eventId = events.ExecuteEvent())
                {
                    if (eventId == EVENT_FLY_AWAY)
                    {
                        Position randomPosOnRadius;
                        randomPosOnRadius.m_positionZ = (me->GetPositionZ() + 40.0f);
                        me->GetNearPoint2D(randomPosOnRadius.m_positionX, randomPosOnRadius.m_positionY, 40.0f, me->GetAngle(me));
                        me->GetMotionMaster()->MovePoint(POINT_FLY_AWAY, randomPosOnRadius);
                    }
                }
            }

            void SpellHit(Unit* /*caster*/, SpellInfo const* spell) override
            {
                switch (spell->Id)
                {
                    case SPELL_EXPLOSION:
                        DoCast(me, SPELL_IMMOLATION);
                        break;
                    case SPELL_RIDE:
                        DoCastAOE(SPELL_PING_BUNNY);
                        events.ScheduleEvent(EVENT_FLY_AWAY, 100);
                        break;
                }
            }
        };

        CreatureAI* GetAI(Creature* creature) const override
        {
            return new npc_frostbrood_skytalonAI(creature);
        }
};

/*######
## The Flesh Giant Champion - Id: 13235
######*/
enum FleshGiant
{
    QUEST_FLESH_GIANT_CHAMPION = 13235,

    NPC_MORBIDUS = 30698,
    NPC_LICH_KING = 31301,
    NPC_OLAKIN = 31428,
    NPC_DHAKAR = 31306,

    FACTION_HOSTILE = 14,
    FACTION_BASIC = 2102,

    EVENT_INTRO = 1,
    EVENT_LK_SAY_1 = 2,
    EVENT_LK_SAY_2 = 3,
    EVENT_LK_SAY_3 = 4,
    EVENT_LK_SAY_4 = 5,
    EVENT_LK_SAY_5 = 6,
    EVENT_OUTRO = 7,
    EVENT_START = 8,

    SPELL_SIMPLE_TELEPORT = 64195,

    SAY_DHAKAR_START = 0,
    SAY_LK_1 = 0,
    SAY_LK_2 = 1,
    SAY_LK_3 = 2,
    SAY_LK_4 = 3,
    SAY_LK_5 = 4,
    SAY_OLAKIN_PAY = 0
};

class npc_margrave_dhakar : public CreatureScript
{
    public:
        npc_margrave_dhakar() : CreatureScript("npc_margrave_dhakar") { }

        struct npc_margrave_dhakarAI : public ScriptedAI
        {
            npc_margrave_dhakarAI(Creature* creature) : ScriptedAI(creature) , _summons(me), _lichKingGuid(0) { }

            void Reset() override
            {
                me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_NONE);

                _events.Reset();
                _summons.DespawnAll();
            }

            void sGossipSelect(Player* player, uint32 sender, uint32 action) override
            {
                if (player->GetQuestStatus(QUEST_FLESH_GIANT_CHAMPION) == QUEST_STATUS_INCOMPLETE && !player->IsInCombat())
                {
                    if (me->GetCreatureTemplate()->GossipMenuId == sender && !action)
                    {
                        _events.ScheduleEvent(EVENT_INTRO, 1000);
                        me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                    }
                }
            }

            void UpdateAI(uint32 diff) override
            {
                _events.Update(diff);

                while (uint32 eventId = _events.ExecuteEvent())
                {
                    switch (eventId)
                    {
                        case EVENT_INTRO:
                        {
                            Talk(SAY_DHAKAR_START);
                            me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_READY2H);

                            if (Creature* morbidus = me->FindNearestCreature(NPC_MORBIDUS, 50.0f, true))
                            {
                                if (Creature* lichKing = me->SummonCreature(NPC_LICH_KING, morbidus->GetPositionX() + 10.0f, morbidus->GetPositionY(), morbidus->GetPositionZ()))
                                {
                                    _lichKingGuid = lichKing->GetGUID();
                                    lichKing->SetFacingTo(morbidus->GetOrientation());
                                    lichKing->CastSpell(lichKing, SPELL_SIMPLE_TELEPORT, true);
                                }
                            }

                            _events.ScheduleEvent(EVENT_LK_SAY_1, 5000);
                            break;
                        }
                        case EVENT_LK_SAY_1:
                        {
                            if (Creature* lichKing = ObjectAccessor::GetCreature(*me, _lichKingGuid))
                                lichKing->AI()->Talk(SAY_LK_1);
                            _events.ScheduleEvent(EVENT_LK_SAY_2, 5000);
                            break;
                        }
                        case EVENT_LK_SAY_2:
                        {
                            if (Creature* lichKing = ObjectAccessor::GetCreature(*me, _lichKingGuid))
                                lichKing->AI()->Talk(SAY_LK_2);
                            _events.ScheduleEvent(EVENT_LK_SAY_3, 5000);
                            break;
                        }
                        case EVENT_LK_SAY_3:
                        {
                            if (Creature* lichKing = ObjectAccessor::GetCreature(*me, _lichKingGuid))
                                lichKing->AI()->Talk(SAY_LK_3);
                            _events.ScheduleEvent(EVENT_LK_SAY_4, 5000);
                            break;
                        }
                        case EVENT_LK_SAY_4:
                        {
                            if (Creature* lichKing = ObjectAccessor::GetCreature(*me, _lichKingGuid))
                                lichKing->AI()->Talk(SAY_LK_4);
                            _events.ScheduleEvent(EVENT_OUTRO, 12000);
                            break;
                        }
                        case EVENT_LK_SAY_5:
                        {
                            if (Creature* lichKing = ObjectAccessor::GetCreature(*me, _lichKingGuid))
                                lichKing->AI()->Talk(SAY_LK_5);
                            _events.ScheduleEvent(EVENT_OUTRO, 8000);
                            break;
                        }
                        case EVENT_OUTRO:
                        {
                            if (Creature* olakin = me->FindNearestCreature(NPC_OLAKIN, 50.0f, true))
                                olakin->AI()->Talk(SAY_OLAKIN_PAY);

                            if (Creature* lichKing = ObjectAccessor::GetCreature(*me, _lichKingGuid))
                                lichKing->DespawnOrUnsummon(0);

                            _events.ScheduleEvent(EVENT_START, 5000);
                            break;
                        }
                        case EVENT_START:
                        {
                            if (Creature* morbidus = me->FindNearestCreature(NPC_MORBIDUS, 50.0f, true))
                            {
                                morbidus->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_ATTACKABLE_1 | UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC | UNIT_FLAG_DISABLE_MOVE);
                                morbidus->setFaction(FACTION_HOSTILE);
								morbidus->SetInCombatWithZone();
                            }

                            break;
                        }
                    }
                }

                DoMeleeAttackIfReady();
            }

        private:
            EventMap _events;
            SummonList _summons;
            uint64 _lichKingGuid;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_margrave_dhakarAI(creature);
    }
};

class npc_morbidus : public CreatureScript
{
    public:
        npc_morbidus() : CreatureScript("npc_morbidus") { }

        struct npc_morbidusAI : public ScriptedAI
        {
            npc_morbidusAI(Creature* creature) : ScriptedAI(creature) { }

            void Reset() override
            {
                if (Creature* dhakar = me->FindNearestCreature(NPC_DHAKAR, 50.0f, true))
                    dhakar->AI()->Reset();

                // this will prevent the event to start without morbidus being alive
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                me->SetReactState(REACT_PASSIVE);
                me->setFaction(FACTION_BASIC);
            }
        };

        CreatureAI* GetAI(Creature* creature) const override
        {
            return new npc_morbidusAI(creature);
        }
};

/*######
## npc_valiant
######*/

enum valiantChampion
{
    SPELL_REFRESH_MOUNT         = 66483,
    SPELL_GIVE_VALIANT_MARK_1   = 62724,
    SPELL_GIVE_VALIANT_MARK_2   = 62770,
    SPELL_GIVE_VALIANT_MARK_3   = 62771,
    SPELL_GIVE_VALIANT_MARK_4   = 62995,
    SPELL_GIVE_VALIANT_MARK_5   = 62996,
    SPELL_GIVE_CHAMPION_MARK    = 63596,
    SPELL_AURA_PERIODIC         = 64223,
    SPELL_BESTED_DARNASSUS      = 64805,
    SPELL_BESTED_GNOMEREGAN     = 64809,
    SPELL_BESTED_IRONFORGE      = 64810,
    SPELL_BESTED_ORGRIMMAR      = 64811,
    SPELL_BESTED_SENJIN         = 64812,
    SPELL_BESTED_SILVERMOON     = 64813,
    SPELL_BESTED_STORMWIND      = 64814,
    SPELL_BESTED_EXODAR         = 64808,
    SPELL_BESTED_UNDERCITY      = 64816,
    SPELL_BESTED_THUNDERBLUFF   = 64815,
    SPELL_AURA_TOURNAMENT_MOUNT = 63034,
};
static const uint32 QuestTheGrandMelee[] = {13665,13745,13750,13756,13761,13767,13772,13777,13782,13787};
static const uint32 QuestAmongTheChampions[] = {13790,13793,13811,13814};

#define GOSSIP_MELEE_FIGHT      "전투할 준비가 되었습니다!"

class npc_valiant : public CreatureScript
{
public:
   npc_valiant() : CreatureScript("npc_valiant") { }

   struct npc_valiantAI : public ScriptedAI
   {
       npc_valiantAI(Creature* creature) : ScriptedAI(creature) {}

       uint32 uiChargeTimer;
       uint32 uiShieldBreakerTimer;
       uint64 guidAttacker;
       bool chargeing;

       void Reset()
       {
           uiChargeTimer = 7*IN_MILLISECONDS;
           uiShieldBreakerTimer = 10*IN_MILLISECONDS;

           me->setFaction(35);
       }

       void EnterCombat(Unit* attacker)
       {
           guidAttacker = attacker->GetGUID();
           DoCast(me,SPELL_AURA_PERIODIC,true);
           if (Aura* aur = me->AddAura(SPELL_RANGED_DEFEND,me))
               aur->ModStackAmount(1);
       }

       void MovementInform(uint32 uiType, uint32 uiId)
       {
           if (uiType != POINT_MOTION_TYPE)
               return;

           if (uiId != 1)
               return;

           chargeing = false;

           DoCastVictim(SPELL_CHARGE);
           if (me->GetVictim())
               me->GetMotionMaster()->MoveChase(me->GetVictim());
       }

       void DamageTaken(Unit* doneby, uint32& uiDamage)
       {
           if (uiDamage > me->GetHealth() && doneby->GetTypeId() == TYPEID_PLAYER)
           {
               uiDamage = 0;

               if (doneby->HasAura(SPELL_AURA_TOURNAMENT_MOUNT))
               {
                   switch(me->GetEntry())
                   {
                   case 33559: // Darnassus
                   case 33562: // Exodar
                   case 33558: // Gnomeregan
                   case 33564: // Ironforge
                   case 33306: // Orgrimmar
                   case 33285: // Sen'jin
                   case 33382: // Silvermoon
                   case 33561: // Stormwind
                   case 33383: // Thunder Bluff
                   case 33384: // Undercity
                       {
                           doneby->CastSpell(doneby,SPELL_GIVE_VALIANT_MARK_1,true);
                           break;
                       }
                   case 33738: // Darnassus
                   case 33739: // Exodar
                   case 33740: // Gnomeregan
                   case 33743: // Ironforge
                   case 33744: // Orgrimmar
                   case 33745: // Sen'jin
                   case 33746: // Silvermoon
                   case 33747: // Stormwind
                   case 33748: // Thunder Bluff
                   case 33749: // Undercity
                       {
                           doneby->CastSpell(doneby,SPELL_GIVE_CHAMPION_MARK,true);
                           break;
                       }
                   }

                  switch(me->GetEntry())
                  {
                      case 33738: // Darnassus
                           doneby->CastSpell(doneby,SPELL_BESTED_DARNASSUS,true); break;
                       case 33739: // Exodar
                           doneby->CastSpell(doneby,SPELL_BESTED_EXODAR,true); break;
                       case 33740: // Gnomeregan
                           doneby->CastSpell(doneby,SPELL_BESTED_GNOMEREGAN,true); break;
                       case 33743: // Ironforge
                           doneby->CastSpell(doneby,SPELL_BESTED_IRONFORGE,true); break;
                       case 33744: // Orgrimmar
                           doneby->CastSpell(doneby,SPELL_BESTED_ORGRIMMAR,true); break;
                       case 33745: // Sen'jin
                           doneby->CastSpell(doneby,SPELL_BESTED_SENJIN,true); break;
                       case 33746: // Silvermoon
                           doneby->CastSpell(doneby,SPELL_BESTED_SILVERMOON,true); break;
                       case 33747: // Stormwind
                           doneby->CastSpell(doneby,SPELL_BESTED_STORMWIND,true); break;
                       case 33748: // Thunder Bluff
                           doneby->CastSpell(doneby,SPELL_BESTED_THUNDERBLUFF,true); break;
                       case 33749: // Undercity
                           doneby->CastSpell(doneby,SPELL_BESTED_UNDERCITY,true); break;
                   }
               }

               me->setFaction(35);
               EnterEvadeMode();
               me->CastSpell(me,SPELL_REFRESH_MOUNT,true);
           }
       }

       void UpdateAI(const uint32 uiDiff)
       {
           if (!UpdateVictim())
               return;

           if (uiChargeTimer <= uiDiff)
           {
               chargeing = true;
               float x,y,z;
               me->GetNearPoint(me, x, y, z, 1.0f, 15.0f, float(2 * M_PI * rand_norm()));
               me->GetMotionMaster()->MovePoint(1,x,y,z);

               uiChargeTimer = 15*IN_MILLISECONDS;
           } else uiChargeTimer -= uiDiff;

           if (uiShieldBreakerTimer <= uiDiff)
           {
               DoCastVictim(SPELL_SHIELD_BREAKER);
               uiShieldBreakerTimer = 10*IN_MILLISECONDS;
           } else uiShieldBreakerTimer -= uiDiff;

           if (me->isAttackReady())
           {
               DoCast(me->GetVictim(), SPELL_PLAYER_THRUST, true);
               me->resetAttackTimer();
           }

           if (Player* player = ObjectAccessor::GetPlayer(*me,guidAttacker))
                if (!player->HasAura(SPELL_AURA_TOURNAMENT_MOUNT))
                    EnterEvadeMode();
       }
   };

   CreatureAI *GetAI(Creature* creature) const
   {
       return new npc_valiantAI(creature);
   }

   bool MakeDuel(Player* player, uint32 npcID)
   {
       switch (npcID)
       {
           case 33738: // Darnassus
               return !player->HasAura(SPELL_BESTED_DARNASSUS);
           case 33739: // Exodar
               return !player->HasAura(SPELL_BESTED_EXODAR);
           case 33740: // Gnomeregan
               return !player->HasAura(SPELL_BESTED_GNOMEREGAN);
           case 33743: // Ironforge
               return !player->HasAura(SPELL_BESTED_IRONFORGE);
           case 33744: // Orgrimmar
               return !player->HasAura(SPELL_BESTED_ORGRIMMAR);
           case 33745: // Sen'jin
               return !player->HasAura(SPELL_BESTED_SENJIN);
           case 33746: // Silvermoon
               return !player->HasAura(SPELL_BESTED_SILVERMOON);
           case 33747: // Stormwind
               return !player->HasAura(SPELL_BESTED_STORMWIND);
           case 33748: // Thunder Bluff
               return !player->HasAura(SPELL_BESTED_THUNDERBLUFF);
           case 33749: // Undercity
               return !player->HasAura(SPELL_BESTED_UNDERCITY);
       }
       return true;
   }

   void AddMeleeFightGossip(Player* player)
   {
       if (!player)
           return;

       if (player->HasAura(SPELL_AURA_TOURNAMENT_MOUNT) &&((player->GetQuestStatus(QuestTheGrandMelee[urand(0,9)])) == QUEST_STATUS_INCOMPLETE))
       {
           player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_MELEE_FIGHT, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
       }
   }


   bool OnGossipHello(Player* player, Creature* creature)
   {
       switch(creature->GetEntry())
       {
       case 33559: // Darnassus
       case 33562: // Exodar
       case 33558: // Gnomeregan
       case 33564: // Ironforge
       case 33561: // Stormwind
          {
               if (player->GetTeamId() == TEAM_ALLIANCE)
                   AddMeleeFightGossip(player);
               break;
           }
       case 33306: // Orgrimmar
       case 33285: // Sen'jin
       case 33382: // Silvermoon
       case 33383: // Thunder Bluff
       case 33384: // Undercity
           {
               if (player->GetTeamId() == TEAM_HORDE)
                   AddMeleeFightGossip(player);
               break;
           }
       case 33738: // Darnassus
       case 33739: // Exodar
       case 33740: // Gnomeregan
       case 33743: // Ironforge
       case 33744: // Orgrimmar
       case 33745: // Sen'jin
       case 33746: // Silvermoon
       case 33747: // Stormwind
       case 33748: // Thunder Bluff
       case 33749: // Undercity
            {
               if (player->HasAura(SPELL_AURA_TOURNAMENT_MOUNT) && ((player->GetQuestStatus(QuestAmongTheChampions[urand(0,3)])) == QUEST_STATUS_INCOMPLETE))
               {
                   if (MakeDuel(player,creature->GetEntry()))
                       player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_MELEE_FIGHT, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
               }
               break;
           }
       }

       player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());
       return true;
   }

   bool OnGossipSelect(Player* player, Creature* creature, uint32 /*uiSender*/, uint32 uiAction)
   {
       player->PlayerTalkClass->ClearMenus();
       player->CLOSE_GOSSIP_MENU();
       if (uiAction == GOSSIP_ACTION_INFO_DEF + 1)
       {
           creature->setFaction(14);
           creature->AI()->AttackStart(player->GetVehicleCreatureBase());
           creature->AddThreat(player, 0.0f);
           creature->SetInCombatWith(player);
           player->SetInCombatWith(creature);
           return true;
       }

       if (uiAction == GOSSIP_ACTION_INFO_DEF + 2)
       {
           creature->setFaction(14);
           creature->AI()->AttackStart(player->GetVehicleCreatureBase());
           creature->AddThreat(player, 0.0f);
           creature->SetInCombatWith(player);
           player->SetInCombatWith(creature);
           return true;
       }
       return true;
   }
};

/*######
## npc_tirion_fordring
######*/

#define GOSSIP_HELLO    "카빈에게 흑기사를 소환해 달라고 청합니다.."

class npc_squire_cavin : public CreatureScript
{
public:
    npc_squire_cavin() : CreatureScript("npc_squire_cavin") { }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        player->PlayerTalkClass->ClearMenus();
        switch (action)
        {
            case GOSSIP_ACTION_INFO_DEF+1:
                player->CLOSE_GOSSIP_MENU();
				creature->SummonCreature(33785, 8432.84f, 960.232f, 544.675f, 6.24278f);
                break;
        }
        return true;
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

		if (player->GetVehicleCreatureBase())
			if (player->GetQuestStatus(13664) == QUEST_STATUS_INCOMPLETE && player->GetVehicleBase()->GetEntry() == 33782)
				player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_HELLO, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+1);

        player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());

        return true;
    }
};


class npc_chillmaw : public CreatureScript
{
    public:
        npc_chillmaw() : CreatureScript("npc_chillmaw") { }

        struct npc_chillmawAI : public ScriptedAI
        {
            npc_chillmawAI(Creature* creature) : ScriptedAI(creature) {}

			uint32 frost_breath;
			bool wing_buffet;
			bool eject_passenger1;
			bool eject_passenger2;
			bool eject_passenger3;

            void Reset() override
            {
                frost_breath = 10*IN_MILLISECONDS;
				wing_buffet = true;
				eject_passenger1 = true;
				eject_passenger2 = true;
				eject_passenger3 = true;
            }

            void DamageTaken(Unit* /*attacker*/, uint32& damage) override
            {
				if (wing_buffet)
					if (me->HealthBelowPctDamaged(35, damage))
					{
					//DoCast(caster, SPELL_MELEE_CREDIT, true);
						DoCastVictim(65260);
						wing_buffet = false;
					}

				if (eject_passenger1)
					if (me->HealthBelowPctDamaged(25, damage))
					{
						DoCast(60603);
						eject_passenger1 = false;
					}

				if (eject_passenger2)
					if (me->HealthBelowPctDamaged(50, damage))
					{
						DoCast(62539);
						eject_passenger2 = false;
					}

				if (eject_passenger3)
					if (me->HealthBelowPctDamaged(75, damage))
					{
						DoCast(52205);
						eject_passenger3 = false;
					}
            }

            void UpdateAI(uint32 diff) override
            {
                if (!UpdateVictim())
                    return;

				if (frost_breath <= diff)
				{
					DoCastVictim(65248);
					frost_breath = 10*IN_MILLISECONDS;
				} else frost_breath -= diff;
            }
        };

        CreatureAI *GetAI(Creature* creature) const
		{
			return new npc_chillmawAI(creature);
		}
};

void AddSC_icecrown()
{
    new npc_squire_david;
    new npc_argent_valiant;
    new npc_guardian_pavilion;
    new npc_vereth_the_cunning;
    new npc_tournament_training_dummy;
    new npc_blessed_banner();
    new npc_frostbrood_skytalon();
    new npc_margrave_dhakar();
    new npc_morbidus();
	new npc_valiant();
	new npc_squire_cavin();

	new npc_chillmaw();
}
