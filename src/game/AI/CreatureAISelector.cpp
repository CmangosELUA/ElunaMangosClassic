/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "CreatureAISelector.h"
#include "Creature.h"
#include "CreatureAIImpl.h"
#include "NullCreatureAI.h"
#include "Policies/Singleton.h"
#include "MovementGenerator.h"
#include "ScriptMgr.h"
#include "Pet.h"
#include "Log.h"
#include "PossessedAI.h"

INSTANTIATE_SINGLETON_1(CreatureAIRegistry);
INSTANTIATE_SINGLETON_1(MovementGeneratorRegistry);

namespace FactorySelector
{
    CreatureAI* selectAI(Creature* creature)
    {
        // Allow scripting AI for normal creatures and not controlled pets (guardians and mini-pets)
        if ((!creature->IsPet() || !((Pet*)creature)->isControlled()) && !creature->isCharmed())
            if (CreatureAI* scriptedAI = sScriptMgr.GetCreatureAI(creature))
                return scriptedAI;

        CreatureAIRegistry& ai_registry(CreatureAIRepository::Instance());

        const CreatureAICreator* ai_factory = nullptr;

        std::string ainame = creature->GetAIName();

        // select by NPC flags _first_ - otherwise EventAI might be choosen for pets/totems
        if (creature->IsPet())
        {
            if (((Pet*)creature)->isControlled())
                ai_factory = ai_registry.GetRegistryItem("PetAI");
            else
            {
                ai_factory = ai_registry.GetRegistryItem("GuardianAI");
                if (!ainame.empty() && (ai_registry.GetRegistryItem(ainame.c_str()) != ai_registry.GetRegistryItem("GuardianAI")))
                    sLog.outErrorDb("FactorySelector: creature pet / guardian not up-to-date on entry: %u ! it shouldn't have %s - GuardianAI will be used.", creature->GetEntry(), ainame.c_str());
            }
        }
        else if (creature->IsTotem())
            ai_factory = ai_registry.GetRegistryItem("TotemAI");
        // select by script name
        else if (!ainame.empty())
            ai_factory = ai_registry.GetRegistryItem(ainame.c_str());
        else if (creature->IsGuard())
            ai_factory = ai_registry.GetRegistryItem("GuardAI");
        // select by permit check
        else
        {
            int best_val = PERMIT_BASE_NO;
            typedef CreatureAIRegistry::RegistryMapType RMT;
            RMT const& l = ai_registry.GetRegisteredItems();
            for (RMT::const_iterator iter = l.begin(); iter != l.end(); ++iter)
            {
                const CreatureAICreator* factory = iter->second;
                const SelectableAI* p = dynamic_cast<const SelectableAI*>(factory);
                MANGOS_ASSERT(p != nullptr);
                int val = p->Permit(creature);
                if (val > best_val)
                {
                    best_val = val;
                    ai_factory = p;
                }
            }
        }

        // select NullCreatureAI if not another cases
        ainame = (ai_factory == nullptr) ? "NullCreatureAI" : ai_factory->key();

        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Creature %u used AI is %s.", creature->GetGUIDLow(), ainame.c_str());
        return (ai_factory == nullptr ? new NullCreatureAI(creature) : ai_factory->Create(creature));
    }

    CreatureAI* GetPossessAI(Creature* creature)
    {
        return new PossessedAI(creature);
    }

    MovementGenerator* selectMovementGenerator(Creature* creature)
    {
        MovementGeneratorRegistry& mv_registry(MovementGeneratorRepository::Instance());
        MANGOS_ASSERT(creature->GetCreatureInfo() != nullptr);
        MovementGeneratorCreator const* mv_factory = mv_registry.GetRegistryItem(
                    creature->GetOwnerGuid().IsPlayer() ? FOLLOW_MOTION_TYPE : creature->GetDefaultMovementType());

        /* if( mv_factory == nullptr  )
        {
            int best_val = -1;
            std::vector<std::string> l;
            mv_registry.GetRegisteredItems(l);
            for( std::vector<std::string>::iterator iter = l.begin(); iter != l.end(); ++iter)
            {
            const MovementGeneratorCreator *factory = mv_registry.GetRegistryItem((*iter).c_str());
            const SelectableMovement *p = dynamic_cast<const SelectableMovement *>(factory);
            ASSERT( p != nullptr );
            int val = p->Permit(creature);
            if( val > best_val )
            {
                best_val = val;
                mv_factory = p;
            }
            }
        }*/

        return (mv_factory == nullptr ? nullptr : mv_factory->Create(creature));
    }
}
