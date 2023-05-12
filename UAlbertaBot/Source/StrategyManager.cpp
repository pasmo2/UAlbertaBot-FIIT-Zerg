#include "Common.h"
#include "StrategyManager.h"
#include "UnitUtil.h"
#include "BaseLocationManager.h"
#include "Global.h"
#include "InformationManager.h"
#include "WorkerManager.h"
#include "Logger.h"
#include <fstream>
#include <stdio.h>
#include <string>
#include "BaseLocationManager.h"


using namespace std;

using namespace UAlbertaBot;
static int lingSpeedGas, carapaceOneGas, meleeAttackOneGas, evo_chamber_made, hydra_den_from_boss = 0;

// constructor
StrategyManager::StrategyManager()
    : m_selfRace(BWAPI::Broodwar->self()->getRace())
    , m_enemyRace(BWAPI::Broodwar->enemy()->getRace())
    , m_emptyBuildOrder(BWAPI::Broodwar->self()->getRace())
{

}



const int StrategyManager::getScore(BWAPI::Player player) const
{
    return player->getBuildingScore() + player->getKillScore() + player->getRazingScore() + player->getUnitScore();
}

const BuildOrder & StrategyManager::getOpeningBookBuildOrder() const
{
    auto buildOrderIt = m_strategies.find(Config::Strategy::StrategyName);

    // look for the build order in the build order map
    if (buildOrderIt != std::end(m_strategies))
    {
        return (*buildOrderIt).second.m_buildOrder;
    }
    else
    {
        UAB_ASSERT_WARNING(false, "Strategy not found: %s, returning empty initial build order", Config::Strategy::StrategyName.c_str());
        return m_emptyBuildOrder;
    }
}

const bool StrategyManager::shouldExpandNow() const
{
    // if there is no place to expand to, we can't expand
    if (Global::Bases().getNextExpansion(BWAPI::Broodwar->self()) == BWAPI::TilePositions::None)
    {
        BWAPI::Broodwar->printf("No valid expansion location");
        return false;
    }

    size_t numDepots    = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Command_Center)
        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Nexus)
        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery)
        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair)
        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive);
    int frame           = BWAPI::Broodwar->getFrameCount();
    int minute          = frame / (24*60);

    // if we have a ton of idle workers then we need a new expansion
    if (Global::Workers().getNumIdleWorkers() > 10)
    {
        return true;
    }

    // if we have a ridiculous stockpile of minerals, expand
    if (BWAPI::Broodwar->self()->minerals() > 1000)
    {
        return true;
    }

    // we will make expansion N after array[N] minutes have passed
    std::vector<int> expansionTimes ={5, 10, 20, 30, 40 , 50};

    std::vector<int> expansionTimesMyExpansions = { 10, 15, 20, 25, 30 , 35 };
    int numOfBasesTaken = 0;
    for (auto& baseLocation : Global::Bases().getBaseLocations())
    {
        if (baseLocation->isOccupiedByPlayer(BWAPI::Broodwar->self())) {
            numOfBasesTaken++;
        }
    }
    //printf("\n picovina: %d\n", numOfBasesTaken);
    for (size_t i(0); i < expansionTimes.size(); ++i)
    {
        if (numDepots < (i+2) && minute >= expansionTimes[i])
        {
            return true;
        }
        else if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg && (Config::Strategy::StrategyName == "7HatchSpeed_NoUpgrades" || Config::Strategy::StrategyName == "7HatchSpeed_Upgrades")) {
            if (minute > expansionTimesMyExpansions[i] && numOfBasesTaken< (i+3)) {
                return true;
            }
        }
    }

    return false;
}

void StrategyManager::addStrategy(const std::string & name, Strategy & strategy)
{
    m_strategies[name] = strategy;
}

const MetaPairVector StrategyManager::getBuildOrderGoal()
{
    BWAPI::Race myRace = BWAPI::Broodwar->self()->getRace();

    if (myRace == BWAPI::Races::Protoss)
    {
        return getProtossBuildOrderGoal();
    }
    else if (myRace == BWAPI::Races::Terran)
    {
        return getTerranBuildOrderGoal();
    }
    else if (myRace == BWAPI::Races::Zerg)
    {
        return getZergBuildOrderGoal();
    }

    return MetaPairVector();
}

const MetaPairVector StrategyManager::getProtossBuildOrderGoal() const
{
    // the goal to return
    MetaPairVector goal;

    int numZealots          = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Zealot);
    int numPylons           = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Pylon);
    int numDragoons         = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Dragoon);
    int numProbes           = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Probe);
    int numNexusCompleted   = BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
    int numNexusAll         = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
    int numCyber            = BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
    int numCannon           = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Photon_Cannon);
    int numScout            = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Corsair);
    int numReaver           = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Reaver);
    int numDarkTeplar       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar);

    if (Config::Strategy::StrategyName == "Protoss_ZealotRush")
    {
        goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Zealot, numZealots + 8));

        // once we have a 2nd nexus start making dragoons
        if (numNexusAll >= 2)
        {
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dragoon, numDragoons + 4));
        }
    }
    else if (Config::Strategy::StrategyName == "Protoss_DragoonRush")
    {
        goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dragoon, numDragoons + 6));
    }
    else if (Config::Strategy::StrategyName == "Protoss_Drop")
    {
        if (numZealots == 0)
        {
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Zealot, numZealots + 4));
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Shuttle, 1));
        }
        else
        {
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Zealot, numZealots + 8));
        }
    }
    else if (Config::Strategy::StrategyName == "Protoss_DTRush")
    {
        goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dark_Templar, numDarkTeplar + 2));

        // if we have a 2nd nexus then get some goons out
        if (numNexusAll >= 2)
        {
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Dragoon, numDragoons + 4));
        }
    }
    else
    {
        UAB_ASSERT_WARNING(false, "Unknown Protoss Strategy Name: %s", Config::Strategy::StrategyName.c_str());
    }

    // if we have 3 nexus, make an observer
    if (numNexusCompleted >= 3)
    {
        goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observer, 1));
    }

    // add observer to the goal if the enemy has cloaked units
    if (Global::Info().enemyHasCloakedUnits())
    {
        goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Robotics_Facility, 1));

        if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Robotics_Facility) > 0)
        {
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observatory, 1));
        }
        if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Observatory) > 0)
        {
            goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Observer, 1));
        }
    }

    // if we want to expand, insert a nexus into the build order
    if (shouldExpandNow())
    {
        goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Nexus, numNexusAll + 1));
    }

    return goal;
}

const MetaPairVector StrategyManager::getTerranBuildOrderGoal() const
{
    // the goal to return
    std::vector<MetaPair> goal;

    int numWorkers      = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_SCV);
    int numCC           = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Command_Center);
    int numMarines      = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Marine);
    int numMedics       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Medic);
    int numWraith       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Wraith);
    int numVultures     = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Vulture);
    int numGoliath      = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Goliath);
    int numTanks        = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode);
    int numBay          = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay);

    if (Config::Strategy::StrategyName == "Terran_MarineRush")
    {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Marine, numMarines + 8));

        if (numMarines > 5)
        {
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Engineering_Bay, 1));
        }
    }
    else if (Config::Strategy::StrategyName == "Terran_4RaxMarines")
    {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Marine, numMarines + 8));
    }
    else if (Config::Strategy::StrategyName == "Terran_VultureRush")
    {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Vulture, numVultures + 8));

        if (numVultures > 8)
        {
            goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, 4));
        }
    }
    else if (Config::Strategy::StrategyName == "Terran_TankPush")
    {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, 6));
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Goliath, numGoliath + 6));
        goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
    }
    else
    {
        BWAPI::Broodwar->printf("Warning: No build order goal for Terran Strategy: %s", Config::Strategy::StrategyName.c_str());
    }



    if (shouldExpandNow())
    {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Command_Center, numCC + 1));
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_SCV, numWorkers + 10));
    }

    return goal;
}

const MetaPairVector StrategyManager::getZergBuildOrderGoal() const
{
    // the goal to return
    std::vector<MetaPair> goal;

    int numWorkers      = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Drone);
    int numCC           = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery)
        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair)
        + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive);
    int numMutas        = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Mutalisk);
    int numDrones       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Drone);
    int zerglings       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Zergling);
    int numHydras       = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk);
    int numScourge      = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Scourge);
    int numGuardians    = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Guardian);

    int mutasWanted = numMutas + 6;
    int hydrasWanted = numHydras + 6;

    //fix the lack of gas/ling speed if starting against zerg - where we skip these to survive a potential ling rush from the opponent
    if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Extractor) == 0) {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Extractor, 1));
    }
    if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Metabolic_Boost) == 0 && BWAPI::Broodwar->self()->gas() > 0) {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Metabolic_Boost, 1));
    }

    if (Config::Strategy::StrategyName == "Zerg_ZerglingRush")
    {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Zergling, zerglings + 6));
    }
    else if (Config::Strategy::StrategyName == "Zerg_2HatchHydra")
    {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Hydralisk, numHydras + 8));
        goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Grooved_Spines, 1));
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Drone, numDrones + 4));
    }
    else if (Config::Strategy::StrategyName == "Zerg_3HatchMuta")
    {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Hydralisk, numHydras + 12));
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Drone, numDrones + 4));
    }
    else if (Config::Strategy::StrategyName == "Zerg_3HatchScourge")
    {
        if (numScourge > 40)
        {
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Hydralisk, numHydras + 12));
        }
        else
        {
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Scourge, numScourge + 12));
        }


        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Drone, numDrones + 4));
    }
    else if (Config::Strategy::StrategyName == "9PoolSpeed")
    {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Zergling, zerglings + 6));
        goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Metabolic_Boost, 1));
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Drone, numDrones + 4));
    }
    else if (Config::Strategy::StrategyName == "7HatchSpeed_NoUpgrades" || Config::Strategy::StrategyName == "7HatchSpeed_Upgrades")
    {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Zergling, zerglings + 4));
        if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spire) > 0) {
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Mutalisk, numMutas + 2));
            goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Zerg_Flyer_Attacks, 1));
        }
        if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair) > 0 && BWAPI::Broodwar->self()->gas() > 300 &&
            BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Carapace) == 1 && 
            BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Melee_Attacks) == 1) {
            goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Zerg_Melee_Attacks, 2));
            goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Zerg_Carapace, 2));
        }
        if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Spire) == 0 && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair) > 0) {
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Spire, 1));
            goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Pneumatized_Carapace, 1));
        }
        //drone up in this interval
        if (BWAPI::Broodwar->getFrameCount()/(24*60) > 10 && BWAPI::Broodwar->getFrameCount() / (24 * 60) < 20) {
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Drone, numDrones + 3));
        }
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Drone, numDrones + 1));
        if (BWAPI::Broodwar->self()->gas() > 104) {
            goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Evolution_Chamber, 1));
            if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Evolution_Chamber)) {
                goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Zerg_Carapace, 1));
                //if +1 armor is done, get +1 melee attacks
                if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair) == 0) {
                    for (auto& unit : BWAPI::Broodwar->self()->getUnits()) {
                        if (unit->getType() == BWAPI::UnitTypes::Zerg_Hatchery) {
                            unit->morph(BWAPI::UnitTypes::Zerg_Lair);
                            break;
                        }
                    }
                }
                if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Carapace) == 1) {
                    goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Zerg_Melee_Attacks, 1));
                }
            }
        }
        if (Config::Strategy::StrategyName == "7HatchSpeed_Upgrades") {
            if (BWAPI::Broodwar->elapsedTime() > 300 && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Evolution_Chamber) == 0) {
                //printf("@@@@@@@@\nEVOCHAMBER TIME\n@@@@@@@@@@@@");
                evo_chamber_made = 1;
                goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Evolution_Chamber, UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Evolution_Chamber) +1));
            }
            if (BWAPI::Broodwar->elapsedTime() > 335) {
                if (meleeAttackOneGas == 0) {
                    //printf("setting melee attack gas");
                    meleeAttackOneGas = 1;
                    Global::Workers().GasThreshold() += 100;//SOMEHOW GET THIS NUMBER FROM BWAPI
                }
                if (carapaceOneGas == 0) {
                    //printf("setting carapace gas");
                    carapaceOneGas = 1;
                    Global::Workers().GasThreshold() += 100;//SOMEHOW GET THIS NUMBER FROM BWAPI
                }
                if (hydra_den_from_boss == 0 && BWAPI::Broodwar->elapsedTime() > 450) {
                    hydra_den_from_boss = 1;
                    //printf("setting hydraden gas");
                    Global::Workers().GasThreshold() += 50;
                }
                if (BWAPI::Broodwar->elapsedTime() > 360 && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Evolution_Chamber)>=1) {
                    if (BWAPI::Broodwar->self()->gas() >= 100) {
                        goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Zerg_Melee_Attacks, 1));
                    }
                    if (BWAPI::Broodwar->self()->gas() >= 100) {
                        goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Zerg_Carapace, 1));
                    }
                }
            }
        }


    }

    if (BWAPI::Broodwar->self()->minerals() > 1000 && BWAPI::Broodwar->self()->gas() < 200 && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Extractor)) {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Extractor, UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Extractor) + 1));
    }

    if (BWAPI::Broodwar->getFrameCount()/(24*60) > 15 && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Extractor) < 2) {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Extractor, UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Extractor) + 1));
    }

    if (BWAPI::Broodwar->getFrameCount() / (24 * 60) > 18 && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Extractor) < 3) {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Extractor, UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Extractor) + 1));
    }

    if (shouldExpandNow())
    {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Hatchery, numCC + 1));
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Drone, numWorkers + 3));
    }

    if (BWAPI::Broodwar->self()->supplyUsed() + 8 >= BWAPI::Broodwar->self()->supplyTotal())
    {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Overlord, UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Overlord) + 1));
    }

    if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Drone) > 30 && BWAPI::Broodwar->self()->minerals() > 1000 && BWAPI::Broodwar->self()->supplyTotal() < 200) {
        goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Zerg_Overlord, UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Overlord)+5));
    }
    return goal;
}

void StrategyManager::readResults()
{
    if (!Config::Modules::UsingStrategyIO)
    {
        return;
    }

    std::string enemyName = BWAPI::Broodwar->enemy()->getName();
    std::replace(enemyName.begin(), enemyName.end(), ' ', '_');

    std::string enemyResultsFile = Config::Strategy::ReadDir + enemyName + ".txt";

    std::string strategyName;
    int wins = 0;
    int losses = 0;

    FILE *file = fopen (enemyResultsFile.c_str(), "r");
    if (file != nullptr)
    {
        char line[4096]; /* or other suitable maximum line size */
        while (fgets (line, sizeof line, file) != nullptr) /* read a line */
        {
            std::stringstream ss(line);

            ss >> strategyName;
            ss >> wins;
            ss >> losses;
            

            //BWAPI::Broodwar->printf("Results Found: %s %d %d", strategyName.c_str(), wins, losses);

            if (m_strategies.find(strategyName) == m_strategies.end())
            {
                //BWAPI::Broodwar->printf("Warning: Results file has unknown Strategy: %s", strategyName.c_str());
            }
            else
            {
                m_strategies[strategyName].m_wins = wins;
                m_strategies[strategyName].m_losses = losses;
            }
        }

        fclose (file);
    }
    else
    {
        BWAPI::Broodwar->printf("No results file found: %s", enemyResultsFile.c_str());
    }
}

void StrategyManager::writeResults()
{
    if (!Config::Modules::UsingStrategyIO)
    {
        return;
    }

    std::string enemyName = BWAPI::Broodwar->enemy()->getName();
    
    std::replace(enemyName.begin(), enemyName.end(), ' ', '_');

    std::string enemyResultsFile = Config::Strategy::WriteDir + enemyName + ".txt";
    //printf("enemyname:  %s\n", enemyName);
    //printf("file:  %s\n",enemyResultsFile);
    std::stringstream ss;

    for (auto & kv : m_strategies)
    {
        const Strategy & strategy = kv.second;

        ss << strategy.m_name << " " << strategy.m_wins << " " << strategy.m_losses << "\n";
    }

    Logger::LogOverwriteToFile(enemyResultsFile, ss.str());
}

void StrategyManager::onEnd(const bool isWinner)
{
    if (!Config::Modules::UsingStrategyIO)
    {
        return;
    }
    hydra_den_from_boss = 0;
    lingSpeedGas = 0;
    carapaceOneGas = 0;
    meleeAttackOneGas = 0;
    evo_chamber_made = 0;
    Global::Workers().initialScout() = 0;
    string MatchOutcome, enemyRace;
    int selfScore, frameCount;
    string Strategy = Config::Strategy::StrategyName;
    const BWAPI::Player enemy = BWAPI::Broodwar->enemy();
    const BWAPI::Player self = BWAPI::Broodwar->self();
    BWAPI::Race enemyRaceEnum = enemy->getRace();
    string mapName = BWAPI::Broodwar->mapFileName();

    selfScore = self->getBuildingScore() + self->getKillScore() + self->getRazingScore() + self->getUnitScore() + self->gatheredGas() + self->gatheredMinerals();
    //printf("buildingscore:%d\nkillscore:%d\nrazingscore:%d\nunitscore:%d\n", self->getBuildingScore(), self->getKillScore(), self->getRazingScore(), self->getUnitScore());

    if (isWinner) {
        MatchOutcome = "Win";
    }
    else {
        MatchOutcome = "Loss";
    }

    if (enemyRaceEnum == BWAPI::Races::Zerg) {
        enemyRace = "Zerg";
    }
    else if (enemyRaceEnum == BWAPI::Races::Terran) {
        enemyRace = "Terran";
    }
    else if (enemyRaceEnum == BWAPI::Races::Protoss) {
        enemyRace = "Protoss";
    }
    else {
        enemyRace = "Unknown";
    }

    frameCount = BWAPI::Broodwar->getFrameCount();

    string frameCountStr = to_string(frameCount), selfScoreStr = to_string(selfScore);


    string myData = MatchOutcome +","+ mapName.c_str() + "," + enemyRace.c_str() + "," + Strategy.c_str() + "," + frameCountStr + "," + selfScoreStr + "\n";


    std::ofstream file;
    file.open("C:/Users/adam/Desktop/scbwdata.csv",std::ios::app);
    file << myData;
    file.close();


    /*string enemyRace = BWAPI::Broodwar->enemy()->getRace().c_str();
    string mapName = BWAPI::Broodwar->mapFileName().c_str();
    string pathStart = "C:/Users/adam/Desktop/scbw_records/Zerg_2HatchHydra/";
    string slash = "/";
    string pathWins = "/wins.txt", pathLosses = "/losses.txt", pathWinrate = "/winrate.txt";
    string fullPathWin = pathStart + mapName + slash + enemyRace + pathWins;
    string fullPathLoss = pathStart + mapName + slash + enemyRace + pathLosses;
    string fullPathWinrateFile = pathStart + mapName + slash + enemyRace + pathWinrate;

    double winrate, wins, losses;
    fstream winFile, lossFile;


    printf("------------- %s ------------------", fullPathWin);
    winFile.open(fullPathWin);
    winFile >> wins;
    winFile.close();
    lossFile.open(fullPathLoss);
    lossFile >> losses;
    lossFile.close();
    ofstream winratefile(fullPathWinrateFile, std::ofstream::trunc);
    winrate = (wins / (wins + losses) * 100);
    winratefile << winrate;
    winratefile.close();
    printf("current winrate: %lf\n", winrate);*/



    if (isWinner)
    {   
        
        m_strategies[Config::Strategy::StrategyName].m_wins++;

       /* int wins;
       fstream wrFile;
        wrFile.open(fullPathWin);
        wrFile >> wins;
        wrFile.close();
        ofstream ofs(fullPathWin, std::ofstream::trunc);
        ofs << wins + 1;
        ofs.close();
        printf("\nwins: %d\n", wins);*/
        //zvz test quick data
        std::string filename = "C:/Users/adam/Desktop/zvztest.txt";
        std::string line = "WIN";
        std::ofstream file(filename, std::ios::app);
        file << line << "\n";
        file.close();


        printf("\nWINNER-------------------------\n");
        
    }
    else
    {
        m_strategies[Config::Strategy::StrategyName].m_losses++;

        /*int losses;
        fstream wrFile;
        wrFile.open(fullPathLoss);
        wrFile >> losses;
        wrFile.close();
        ofstream ofs(fullPathLoss, std::ofstream::trunc);
        ofs << losses + 1;
        ofs.close();
        printf("\nlosses: %d\n", losses);*/

        //zvz test quick data
        std::string filename = "C:/Users/adam/Desktop/zvztest.txt";
        std::string line = "LOSS";
        std::ofstream file(filename, std::ios::app);
        file << line << "\n";
        file.close();

        printf("\nLOSER-------------------------\n");
    }
    writeResults();

}

void StrategyManager::setLearnedStrategy()
{
    // we are currently not using this functionality for the competition so turn it off 
    return;

    if (!Config::Modules::UsingStrategyIO)
    {
        return;
    }

    const std::string & strategyName = Config::Strategy::StrategyName;
    Strategy & currentStrategy = m_strategies[strategyName];

    int totalGamesPlayed = 0;
    int strategyGamesPlayed = currentStrategy.m_wins + currentStrategy.m_losses;
    double winRate = strategyGamesPlayed > 0 ? currentStrategy.m_wins / static_cast<double>(strategyGamesPlayed) : 0;

    // if we are using an enemy specific strategy
    if (Config::Strategy::FoundEnemySpecificStrategy)
    {
        return;
    }

    // if our win rate with the current strategy is super high don't explore at all
    // also we're pretty confident in our base strategies so don't change if insufficient games have been played
    if (strategyGamesPlayed < 5 || (strategyGamesPlayed > 0 && winRate > 0.49))
    {
        BWAPI::Broodwar->printf("Still using default strategy");
        return;
    }

    // get the total number of games played so far with this race
    for (auto & kv : m_strategies)
    {
        Strategy & strategy = kv.second;
        if (strategy.m_race == BWAPI::Broodwar->self()->getRace())
        {
            totalGamesPlayed += strategy.m_wins + strategy.m_losses;
        }
    }

    // calculate the UCB value and store the highest
    double C = 0.5;
    std::string bestUCBStrategy;
    double bestUCBStrategyVal = std::numeric_limits<double>::lowest();
    for (auto & kv : m_strategies)
    {
        Strategy & strategy = kv.second;
        if (strategy.m_race != BWAPI::Broodwar->self()->getRace())
        {
            continue;
        }

        int sGamesPlayed = strategy.m_wins + strategy.m_losses;
        double sWinRate = sGamesPlayed > 0 ? currentStrategy.m_wins / static_cast<double>(strategyGamesPlayed) : 0;
        double ucbVal = C * sqrt(log((double)totalGamesPlayed / sGamesPlayed));
        double val = sWinRate + ucbVal;

        if (val > bestUCBStrategyVal)
        {
            bestUCBStrategy = strategy.m_name;
            bestUCBStrategyVal = val;
        }
    }

    Config::Strategy::StrategyName = bestUCBStrategy;
}