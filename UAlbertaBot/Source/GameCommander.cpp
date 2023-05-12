#include "Common.h"
#include "GameCommander.h"
#include "UnitUtil.h"
#include "BaseLocationManager.h"
#include "Global.h"
#include "BuildingPlacerManager.h"
#include "BOSSManager.h"
#include "MapTools.h"
#include "InformationManager.h"
#include "WorkerManager.h"
#include "ProductionManager.h"
#include "BuildingManager.h"
#include "ScoutManager.h"
#include "StrategyManager.h"
#include "Squad.h"
#include "BaseLocation.h"
#include <cstdlib>
#include <ctime>

using namespace UAlbertaBot;

GameCommander::GameCommander() 
    : m_initialScoutSet(false)
{

}

void GameCommander::update()
{
    PROFILE_FUNCTION();

	m_timerManager.startTimer(TimerManager::All);

	// populate the unit vectors we will pass into various managers
	handleUnitAssignments();

	// utility managers
	m_timerManager.startTimer(TimerManager::InformationManager);
	Global::Info().update();
	m_timerManager.stopTimer(TimerManager::InformationManager);

	m_timerManager.startTimer(TimerManager::MapTools);
	Global::Map().onFrame();
	m_timerManager.stopTimer(TimerManager::MapTools);

	// economy and base managers
	m_timerManager.startTimer(TimerManager::Worker);
	Global::Workers().onFrame();
	m_timerManager.stopTimer(TimerManager::Worker);

	m_timerManager.startTimer(TimerManager::Production);
	Global::Production().update();
	m_timerManager.stopTimer(TimerManager::Production);

	// combat and scouting managers
	m_timerManager.startTimer(TimerManager::Combat);
	m_combatCommander.update(m_combatUnits);
	m_timerManager.stopTimer(TimerManager::Combat);

	m_timerManager.startTimer(TimerManager::Scout);
    Global::Scout().update();
	m_timerManager.stopTimer(TimerManager::Scout);

	m_timerManager.stopTimer(TimerManager::All);

	Global::Bases().onFrame();

	drawDebugInterface();
}

void GameCommander::drawDebugInterface()
{
	Global::Info().drawExtendedInterface();
	Global::Info().drawUnitInformation(425,30);
	Global::Info().drawMapInformation();
	
	Global::Production().drawProductionInformation(30, 50);
    
	m_combatCommander.drawSquadInformation(200, 30);
    m_timerManager.displayTimers(490, 225);
    drawGameInformation(4, 1);

	// draw position of mouse cursor
	if (Config::Debug::DrawMouseCursorInfo)
	{
		int mouseX = BWAPI::Broodwar->getMousePosition().x + BWAPI::Broodwar->getScreenPosition().x;
		int mouseY = BWAPI::Broodwar->getMousePosition().y + BWAPI::Broodwar->getScreenPosition().y;
		BWAPI::Broodwar->drawTextMap(mouseX + 20, mouseY, " %d %d", mouseX, mouseY);
	}
}

void GameCommander::drawGameInformation(int x, int y)
{
    BWAPI::Broodwar->drawTextScreen(x, y, "\x04Players:");
	BWAPI::Broodwar->drawTextScreen(x+50, y, "%c%s \x04vs. %c%s", BWAPI::Broodwar->self()->getTextColor(), BWAPI::Broodwar->self()->getName().c_str(), 
                                                                  BWAPI::Broodwar->enemy()->getTextColor(), BWAPI::Broodwar->enemy()->getName().c_str());
	y += 12;
		
    BWAPI::Broodwar->drawTextScreen(x, y, "\x04Strategy:");
	BWAPI::Broodwar->drawTextScreen(x+50, y, "\x03%s %s", Config::Strategy::StrategyName.c_str(), Config::Strategy::FoundEnemySpecificStrategy ? "(enemy specific)" : "");
	BWAPI::Broodwar->setTextSize();
	y += 12;

    BWAPI::Broodwar->drawTextScreen(x, y, "\x04Map:");
	BWAPI::Broodwar->drawTextScreen(x+50, y, "\x03%s", BWAPI::Broodwar->mapFileName().c_str());
	BWAPI::Broodwar->setTextSize();
	y += 12;

    BWAPI::Broodwar->drawTextScreen(x, y, "\x04Time:");
    BWAPI::Broodwar->drawTextScreen(x+50, y, "\x04%d %4dm %3ds", BWAPI::Broodwar->getFrameCount(), (int)(BWAPI::Broodwar->getFrameCount()/(23.8*60)), (int)((int)(BWAPI::Broodwar->getFrameCount()/23.8)%60));
}

// assigns units to various managers
void GameCommander::handleUnitAssignments()
{
	m_validUnits.clear();
    m_combatUnits.clear();
	// filter our units for those which are valid and usable
	setValidUnits();

	// set each type of unit
	if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg && BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg) {
		if (BWAPI::Broodwar->elapsedTime()>60) {
			setScoutUnits();
		}
	}
	else if(!(BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg) || BWAPI::Broodwar->elapsedTime()>200) {
		setScoutUnits();
	}
	setCombatUnits();
}

bool GameCommander::isAssigned(BWAPI::Unit unit) const
{
	return m_combatUnits.contains(unit) || m_scoutUnits.contains(unit);
}

// validates units as usable for distribution to various managers
void GameCommander::setValidUnits()
{
	// make sure the unit is completed and alive and usable
	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (UnitUtil::IsValidUnit(unit))
		{	
			m_validUnits.insert(unit);
		}
	}
}

void GameCommander::setScoutUnits()
{
    // if we haven't set a scout unit, do it
    if (m_scoutUnits.empty() && !m_initialScoutSet)
    {
        BWAPI::Unit supplyProvider = getFirstSupplyProvider();

		// if it exists
		if (supplyProvider)
		{
			printf("%d\n", std::rand());
			// grab the closest worker to the supply provider to send to scout
			BWAPI::Unit workerScout = getClosestWorkerToTarget(supplyProvider->getPosition());

			// if we find a worker (which we should) add it to the scout units
			if (workerScout)
			{
                Global::Scout().setWorkerScout(workerScout);
				assignUnit(workerScout, m_scoutUnits);
                m_initialScoutSet = true;
			}
		}
    }
}

// sets combat units to be passed to CombatCommander
void GameCommander::setCombatUnits()
{
	for (auto & unit : m_validUnits)
	{
		if (!isAssigned(unit) && UnitUtil::IsCombatUnit(unit) || unit->getType().isWorker())		
		{	
			assignUnit(unit, m_combatUnits);
		}
	}
}

BWAPI::Unit GameCommander::getFirstSupplyProvider()
{
	BWAPI::Unit supplyProvider = nullptr;

	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg)
	{
		if (Config::Strategy::StrategyName == "7HatchSpeed_NoUpgrades") {
			for (auto& unit : BWAPI::Broodwar->self()->getUnits())
			{
				if (unit->getType() == BWAPI::UnitTypes::Zerg_Hatchery)
				{
					supplyProvider = unit;
				}
			}
		}
		else {
			for (auto& unit : BWAPI::Broodwar->self()->getUnits())
			{
				if (unit->getType() == BWAPI::UnitTypes::Zerg_Spawning_Pool)
				{
					supplyProvider = unit;
				}
			}
		}
	}
	else
	{
		for (auto & unit : BWAPI::Broodwar->self()->getUnits())
		{
			if (unit->getType() == BWAPI::Broodwar->self()->getRace().getSupplyProvider())
			{
				supplyProvider = unit;
			}
		}
	}

	return supplyProvider;
}

void GameCommander::onUnitShow(BWAPI::Unit unit)			
{ 
	Global::Info().onUnitShow(unit); 
	Global::Workers().onUnitShow(unit);
}

void GameCommander::onUnitHide(BWAPI::Unit unit)			
{ 
	Global::Info().onUnitHide(unit); 
}

void GameCommander::onUnitCreate(BWAPI::Unit unit)		
{ 
	Global::Info().onUnitCreate(unit);
	if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord) {
		for (unsigned int i = 0; i < Global::Workers().overlords_vector.size(); i++) {
			if (Global::Workers().overlords_vector[i].unit_identification->getID() == unit->getID()) {
				return;
			}
		}
		OverlordInfo newOverlord;
		newOverlord.unit_identification = unit;
		newOverlord.role = "unset";
		Global::Workers().overlords_vector.push_back(newOverlord);
		// for all bases
		int last_unset_overlord_index = 0;
		for (auto& base : Global::Bases().getBaseLocations()) {
			// if base location is not start, is not occupied by self or enemy
			if ((!(base->isStartLocation()) && (BWAPI::Broodwar->getFrameCount() / (24 * 60) < 5))
				&& !(base->isOccupiedByPlayer(BWAPI::Broodwar->self())) && !(base->isOccupiedByPlayer(BWAPI::Broodwar->enemy()))) {
				// figure out if the base is already set for an overlord, if not, add it to the first unset one
				last_unset_overlord_index = -1;
				for (unsigned int i = 0; i < Global::Workers().overlords_vector.size(); i++) {
					if (Global::Workers().overlords_vector[i].role == "unset") {
						last_unset_overlord_index = i;
					}
					if (Global::Workers().overlords_vector[i].role == std::to_string(base->m_baseID)) {
						//mark last unset overlord index as -1, which means I am not putting this base into an another overlord
						last_unset_overlord_index = -1;
						break;
					}
				}
				if (last_unset_overlord_index != -1) {
					Global::Workers().overlords_vector[last_unset_overlord_index].role = std::to_string(base->m_baseID);
					Global::Workers().overlords_vector[last_unset_overlord_index].assigned_base = base->getPosition();
				}
			}
		}
	}
}

void GameCommander::onUnitComplete(BWAPI::Unit unit)
{
	Global::Info().onUnitComplete(unit);
}

void GameCommander::onUnitRenegade(BWAPI::Unit unit)		
{ 
	Global::Info().onUnitRenegade(unit); 
}

void GameCommander::onUnitDestroy(BWAPI::Unit unit)		
{ 	
	Global::Production().onUnitDestroy(unit);
	Global::Workers().onUnitDestroy(unit);
	Global::Info().onUnitDestroy(unit);

	if (BWAPI::Broodwar->self()->supplyUsed() < 260 && Config::Micro::UseSparcraftSimulation == false) {
		Config::Micro::UseSparcraftSimulation = true;
		printf("TURNING ON SPARCRAFT --- UNIT COUNT BECAME TOO LOW\n");
	}

	//remove overlord from overlord management vector
	if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord) {
		for (unsigned int i = 0; i < Global::Workers().overlords_vector.size(); i++) {
			if (Global::Workers().overlords_vector[i].unit_identification->getID() == unit->getID()) {
				/*printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@ before remove: @@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
				for (int j = 0; j < Global::Workers().overlords_vector.size(); j++) {
					printf("%d\n", Global::Workers().overlords_vector[j].unit_identification->getID());
					printf("%s\n", Global::Workers().overlords_vector[j].unit_identification->getType().getName().c_str());
				}*/
				Global::Workers().overlords_vector.erase(Global::Workers().overlords_vector.begin() + i);
				/*printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@ after remove: @@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
				for (int j = 0; j < Global::Workers().overlords_vector.size(); j++) {
					printf("%d\n", Global::Workers().overlords_vector[j].unit_identification->getID());
					printf("%s\n", Global::Workers().overlords_vector[j].unit_identification->getType().getName().c_str());
				}*/
				break;
			}
		}
	}

	if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord && unit->getPlayer() == BWAPI::Broodwar->self()) {
		Global::Production().m_queue.queueAsHighestPriority(UAlbertaBot::MetaType(BWAPI::UnitTypes::Zerg_Overlord), true, false);
		printf("OVERLORD DIED, ADDING ONE TO THE QUEUE ASAP\n");
	}

	if (unit->getPlayer() == BWAPI::Broodwar->enemy() && (unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar || unit->getType() == BWAPI::UnitTypes::Zerg_Lurker) &&
		BWAPI::Broodwar->self()->supplyUsed() < 260) {
		printf("\nTURUNING SPARCRAFT BACK ON\n");
		Config::Micro::UseSparcraftSimulation = true;
	}

	//if a unit dies - check if there is a cloaked unit somewhere in enemies army - if so, send a squad overlord to its position
	for (auto& enemyUnit : BWAPI::Broodwar->enemy()->getUnits()) {
		if (enemyUnit->getType() == BWAPI::UnitTypes::Zerg_Lurker || 
			enemyUnit->getType() == BWAPI::UnitTypes::Terran_Ghost || 
			enemyUnit->getType() == BWAPI::UnitTypes::Terran_Wraith ||
			enemyUnit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar) {
			for (auto& myUnit: BWAPI::Broodwar->self()->getUnits()) {
				if (myUnit->getDistance(enemyUnit->getPosition()) < 200) {
					for (auto& overlord : Global::Workers().overlords_vector) {
						if (overlord.role == "squad") {
							overlord.unit_identification->move(enemyUnit->getPosition());
							printf("\n@@@@@@@@@@@@@@@@@@@\nMOVING SQUAD OVERLORD TO CLOAKED UNIT\n@@@@@@@@@@@@@@@@@@\n");
							Config::BWAPIOptions::SetLocalSpeed = 50;
							printf("\nTURNING SPARCRAFT OFF \n");
							Config::Micro::UseSparcraftSimulation = false;
							break;
						}
					}
				}
			}
		}
	}
}


void GameCommander::onUnitMorph(BWAPI::Unit unit)		
{ 
	Global::Info().onUnitMorph(unit);
	Global::Workers().onUnitMorph(unit);

	//if morphed unit gets us to over 150 supply, disable SparCraft
	if (BWAPI::Broodwar->self()->supplyUsed() > 260 && Config::Micro::UseSparcraftSimulation == true) {
		Config::Micro::UseSparcraftSimulation = false;
		printf("TURNING OFF SPARCRAFT --- TOO MANY UNITS TO HANDLE\n");
	}


	if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord) {
		for (unsigned int i = 0; i < Global::Workers().overlords_vector.size(); i++) {
			if (Global::Workers().overlords_vector[i].unit_identification->getID() == unit->getID()) {
				return;
			}
		}
		OverlordInfo newOverlord;
		newOverlord.unit_identification = unit;
		newOverlord.role = "unset";
		Global::Workers().overlords_vector.push_back(newOverlord);
		// for all bases
		int last_unset_overlord_index = 0;
		int existing_squad_overlord = 0;
		for (auto& overlord : Global::Workers().overlords_vector) {
			if (overlord.role == "squad") {
				existing_squad_overlord++;
			}
		}
		//printf("\nbase search:\n");
		for (auto& base : Global::Bases().getBaseLocations()) {
			// if base location is not start, is not occupied by self or enemy
			if ((!(base->isStartLocation()) && ((BWAPI::Broodwar->getFrameCount() / (24 * 60) < 5) || existing_squad_overlord > 0))
				&& !(base->isOccupiedByPlayer(BWAPI::Broodwar->enemy()))) {
				//printf("available bases -> %d\n",base->m_baseID);
				// figure out if the base is already set for an overlord, if not, add it to the first unset one
				last_unset_overlord_index = -1;
				for (unsigned int i = 0; i < Global::Workers().overlords_vector.size(); i++) {
					if (Global::Workers().overlords_vector[i].role == "unset") {
						last_unset_overlord_index = i;
					}
					if (Global::Workers().overlords_vector[i].role == std::to_string(base->m_baseID)) {
						//mark last unset overlord index as -1, which means I am not putting this base into an another overlord
						last_unset_overlord_index = -1;
						break;
					}
				}
				if (last_unset_overlord_index != -1) {
					//printf("setting base %d\n", base->m_baseID);
					Global::Workers().overlords_vector[last_unset_overlord_index].role = std::to_string(base->m_baseID);
					Global::Workers().overlords_vector[last_unset_overlord_index].assigned_base = base->getPosition();
				}
			}
		}

		if (existing_squad_overlord > 1) {
			for (auto& overlord : Global::Workers().overlords_vector) {
				if (overlord.role == "unset") {
					overlord.role = "wanderer";
				}
			}
		}

		/*printf("\nprinting overlords:\n");
		for (auto& overlord : Global::Workers().overlords_vector) {
			printf("%s\n",overlord.role.c_str());
		}*/

	}
}

BWAPI::Unit GameCommander::getClosestUnitToTarget(BWAPI::UnitType type, BWAPI::Position target)
{
	BWAPI::Unit closestUnit = nullptr;
	double closestDist = 100000;

	for (auto & unit : m_validUnits)
	{
		if (unit->getType() == type)
		{
			double dist = unit->getDistance(target);
			if (!closestUnit || dist < closestDist)
			{
				closestUnit = unit;
				closestDist = dist;
			}
		}
	}

	return closestUnit;
}

BWAPI::Unit GameCommander::getClosestWorkerToTarget(BWAPI::Position target)
{
	BWAPI::Unit closestUnit = nullptr;
	double closestDist = 100000;

	for (auto & unit : m_validUnits)
	{
		if (!isAssigned(unit) && unit->getType().isWorker() && Global::Workers().isFree(unit))
		{
			double dist = unit->getDistance(target);
			if (!closestUnit || dist < closestDist)
			{
				closestUnit = unit;
				closestDist = dist;
			}
		}
	}

	return closestUnit;
}

void GameCommander::assignUnit(BWAPI::Unit unit, BWAPI::Unitset & set)
{
    if (m_scoutUnits.contains(unit)) { m_scoutUnits.erase(unit); }
    else if (m_combatUnits.contains(unit)) { m_combatUnits.erase(unit); }

    set.insert(unit);
}
