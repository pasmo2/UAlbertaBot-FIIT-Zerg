#include "Common.h"
#include "WorkerManager.h"
#include "Micro.h"
#include "BaseLocationManager.h"
#include "Global.h"
#include "BuildingData.h"
#include "UnitUtil.h"
#include "ScoutManager.h"
#include "GameCommander.h"
#include "Global.h"
#include "MapTools.h"

using namespace UAlbertaBot;


WorkerManager::WorkerManager()
{
    int overlord_handling_call_count = 0;
    //gasThreshold = 0;
}

//void WorkerManager::increaseGasThreshold(int amount) {
//    gasThreshold = gasThreshold + amount;
//}

void WorkerManager::onFrame()
{
    PROFILE_FUNCTION();

    updateWorkerStatus();
    handleGasWorkers();
    handleIdleWorkers();
    handleMoveWorkers();
    handleCombatWorkers();

    if (Config::Strategy::StrategyName == "7HatchSpeed_NoUpgrades" || Config::Strategy::StrategyName == "7HatchSpeed_Upgrades") {
        if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Creep_Colony) > 0 && BWAPI::Broodwar->self()->minerals() > 50) {
            for (auto& unit : BWAPI::Broodwar->self()->getUnits()) {
                if (unit->getType() == BWAPI::UnitTypes::Zerg_Creep_Colony) {
                    unit->morph(BWAPI::UnitTypes::Zerg_Sunken_Colony);
                }
            }
        }
        if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery)>1) {
            createSpine();
        }
    }

    //if ((BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss || BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran) && Global::Scout().m_workerScout) {
    //    //printf("scout pos: %d, %d\n", Global::Scout().m_workerScout->getPosition().x, Global::Scout().m_workerScout->getPosition().x);
    //    printf("working\n");
    //    int countenemyunits = 0;
    //    for (auto& unit : BWAPI::Broodwar->enemy()->getUnits()) {
    //            printf("enemy unit -> %s\n",unit->getType().getName().c_str());
    //            countenemyunits++;
    //    }
    //    int outcome;
    //    if (countenemyunits > 0) {
    //        printf("releasing scout\n");
    //        outcome = Global::Scout().m_workerScout->move(BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation().x*32, BWAPI::Broodwar->self()->getStartLocation().y*32));
    //        printf("outcome: %d\n", outcome);
    //        Global::Scout().releaseScout(Global::Scout().m_workerScout);
    //    }
    //}

    if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery)<6) {
        createMacroHatch();
    }

    if (BWAPI::Broodwar->elapsedTime() > 10 * overlord_handling_call_count) {
        overlordHandling();
    }

    drawResourceDebugInfo();
    drawWorkerInformation(450, 20);

    m_workerData.drawDepotDebugInfo();

    handleRepairWorkers();
}

void WorkerManager::instantScout() {
    if (Global::Workers().initialScout() == 0) {
        for (auto& unit : BWAPI::Broodwar->self()->getUnits()) {
            if (unit->getType() == BWAPI::UnitTypes::Zerg_Drone) {
                Global::Scout().setWorkerScout(unit);
                //assignUnit(workerScout, m_scoutUnits);
                //m_initialScoutSet = true;
                Global::Scout().moveScouts();
                Global::Workers().initialScout() = 1;
                break;
            }
        }
    }
}

void WorkerManager::overlordHandling() {

    /*for (auto& base : Global::Bases().getBaseLocations()) {
        if (base->isStartLocation() || base->isOccupiedByPlayer(BWAPI::Broodwar->enemy()) || base->isOccupiedByPlayer(BWAPI::Broodwar->self())) {
            continue;
        }
        if (BWAPI::Broodwar->isVisible(base->getDepotPosition())) {
            continue;
        }
        for (auto& unit : BWAPI::Broodwar->self()->getUnits()) {
            if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord && !unit->isMoving()) {
                for (auto& myBase : Global::Bases().getOccupiedBaseLocations(BWAPI::Broodwar->self())) {
                    if (unit->getPosition().x <= myBase->getPosition().x+400 && unit->getPosition().x >= myBase->getPosition().x- 400 && unit->getPosition().y <= myBase->getPosition().y + 400 && unit->getPosition().y >= myBase->getPosition().y - 400) {
                            unit->move(base->getPosition());
                            break;
                    }
                }
            }
        }
    }*/
    BWAPI::Position basePos;
    for (auto& overlord : Global::Workers().overlords_vector) {
        if (!(overlord.role[0] >='0' && overlord.role[0] <= '9') && overlord.role!="wanderer") {
            continue;
        }
        basePos = overlord.assigned_base;
        if (!(overlord.unit_identification->isMoving()) && 
            !(overlord.unit_identification->getPosition().x <= basePos.x + 400 && overlord.unit_identification->getPosition().x >= basePos.x - 400 && overlord.unit_identification->getPosition().y <= basePos.y + 400 && overlord.unit_identification->getPosition().y >= basePos.y - 400)) {
            overlord.unit_identification->move(basePos);
        }
        if (overlord.role == "wanderer" && !overlord.unit_identification->isMoving()) {
            QueueWandererMovement(overlord.unit_identification);
        }
    }

}


void WorkerManager::QueueWandererMovement(BWAPI::Unit wanderer) {
    int width = Global::Map().width();
    int height = Global::Map().height();
    int min = 1;
    int randomX = 0;
    int randomY = 0;
    BWAPI::TilePosition generatedTilePos;
    BWAPI::Position generatedPos;
    std::vector<BWAPI::Unit> enemyBases;
    int enemyBasesValidPos = 1;

    //   divided by two to be middle of the map, multiplied by 32 to be pixels instead of tiles roughly
    wanderer->move(BWAPI::Position(width * 16, height * 16), true);

    for (auto& enemyUnit : BWAPI::Broodwar->enemy()->getUnits()) {
        if (enemyUnit->getType() == BWAPI::UnitTypes::Zerg_Hatchery || enemyUnit->getType() == BWAPI::UnitTypes::Protoss_Nexus || enemyUnit->getType() == BWAPI::UnitTypes::Terran_Command_Center) {
            enemyBases.push_back(enemyUnit);
        }
    }


    for (int i = 0; i < 10; i++) {
        randomX = min + std::rand() % (width - min + 1);
        randomY = min + std::rand() % (height - min + 1);
        generatedTilePos = BWAPI::TilePosition(randomX, randomY);
        for (auto& base : enemyBases) {
            if (generatedTilePos.getDistance(base->getTilePosition()) < 30) {
                enemyBasesValidPos = 0;
            }
        }
        if (enemyBasesValidPos == 1 && Global::Map().isValidTile(generatedTilePos)) {
            generatedPos = BWAPI::Position(generatedTilePos.x * 32, generatedTilePos.y * 32);
            wanderer->move(generatedPos, true);
        }
    }
}


bool WorkerManager::tileHasUnitNearby(BWAPI::TilePosition pos) {
    int plusminus = 400;
    for (auto& unit : BWAPI::Broodwar->self()->getUnits()) {
        if (unit->getPosition().x <= (pos.x + plusminus) && unit->getPosition().x >= (pos.x - plusminus) && unit->getPosition().y <= (pos.y + plusminus) && unit->getPosition().y >= (pos.y - plusminus)) {
            return true;
        }
    }
    return false;
}

void WorkerManager::createSpine() {
    int def_structure_count = 0;
    def_structure_count += UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Creep_Colony) + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Sunken_Colony);

    if (BWAPI::Broodwar->getFrameCount() > 24 * 60 * 3 && BWAPI::Broodwar->self()->minerals() > 75 && def_structure_count<3) {

        const BaseLocation* homeBase = Global::Bases().getPlayerStartingBaseLocation(BWAPI::Broodwar->self());
        const BaseLocation* closestBase = nullptr;
        BWAPI::TilePosition existing_spine_pos;
        int minDistance = std::numeric_limits<int>::max();
        //find natural
        for (auto& base : Global::Bases().getBaseLocations()) {

            if (base->getDepotPosition() == homeBase->getDepotPosition()) {
                continue;
            }
            
            int distanceFromHome = homeBase->getGroundDistance(base->getDepotPosition());

            if (distanceFromHome < 0) {
                continue;
            }

            if (!closestBase || distanceFromHome < minDistance) {
                closestBase = base;
                minDistance = distanceFromHome;
            }
        }
        int checkForBaseInNatural = 0;
        //check if natural has a hatchery, if not, just quit because there is little point making sunkens in the starting base
        for (auto& unit : BWAPI::Broodwar->self()->getUnits()) {
            if (unit->getType() == BWAPI::UnitTypes::Zerg_Hatchery && unit->isCompleted()) {
                if (unit->getPosition().getApproxDistance(closestBase->getPosition()) < 500) {
                    checkForBaseInNatural = 1;
                }
            }
            else if (unit->getType() == BWAPI::UnitTypes::Zerg_Creep_Colony || unit->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony) {
                existing_spine_pos = unit->getTilePosition();
            }
        }
        if(checkForBaseInNatural==0){
            return;
        }

        BWAPI::Unit creepColonyDrone;
        bool outcome;
        // For each unit that we own
        for (auto& unit : BWAPI::Broodwar->self()->getUnits())
        {
            // if the unit is of the correct type, and it actually has been constructed, return it
            if (unit->getType() == BWAPI::UnitTypes::Zerg_Drone && unit->isCompleted())
            {
                creepColonyDrone = unit;
                break;
            }
        }
        if (creepColonyDrone) {
            if (def_structure_count == 0) {
                BWAPI::TilePosition creepColonyPos = BWAPI::Broodwar->getBuildLocation(BWAPI::UnitTypes::Zerg_Creep_Colony, closestBase->getDepotPosition(), 10, true);
                outcome = creepColonyDrone->build(BWAPI::UnitTypes::Zerg_Creep_Colony, creepColonyPos);
            }
            else {
                if (existing_spine_pos) {
                    BWAPI::TilePosition creepColonyPos = BWAPI::Broodwar->getBuildLocation(BWAPI::UnitTypes::Zerg_Creep_Colony, existing_spine_pos, 5, true);
                    creepColonyDrone->build(BWAPI::UnitTypes::Zerg_Creep_Colony, creepColonyPos);
                }
            }
        }
    }
}

void WorkerManager::createMacroHatch() {
    int outcome = false;
    if (BWAPI::Broodwar->self()->minerals() > 400) {
        //printf("@@@@@@@@@@@@@@@@@@@@@@@@@@\nmacro hatch time!!!\n@@@@@@@@@@@@@@@@@@@@@@@\n");

        BWAPI::Unit macroHatchDrone;
        // For each unit that we own
        for (auto& unit : BWAPI::Broodwar->self()->getUnits())
        {
            // if the unit is of the correct type, and it actually has been constructed, return it
            if (unit->getType() == BWAPI::UnitTypes::Zerg_Drone && unit->isCompleted())
            {
                macroHatchDrone = unit;
                break;
            }
        }
        if (macroHatchDrone) {
            //printf("\nmacro hatch drone acquired\n");
            BWAPI::TilePosition startPos = BWAPI::Broodwar->self()->getStartLocation();
            BWAPI::TilePosition macroHatchPos = BWAPI::Broodwar->getBuildLocation(BWAPI::UnitTypes::Zerg_Hatchery, startPos, 200, false);
            if (macroHatchPos == BWAPI::TilePositions::Invalid) {
                //printf("finding pos failed\n");
            }
            outcome = macroHatchDrone->build(BWAPI::UnitTypes::Zerg_Hatchery, macroHatchPos);
        }
        /*Global::Workers().numOfMacroHatcheries() += 1;
        printf("creating macro hatch\n\n");*/
    }
}

void WorkerManager::updateWorkerStatus()
{
    PROFILE_FUNCTION();

    // for each of our Workers
    for (auto & worker : m_workerData.getWorkers())
    {
        if (!worker->isCompleted())
        {
            continue;
        }

        // if it's idle
        if (worker->isIdle() &&
            (m_workerData.getWorkerJob(worker) != WorkerData::Build) &&
            (m_workerData.getWorkerJob(worker) != WorkerData::Move) &&
            (m_workerData.getWorkerJob(worker) != WorkerData::Scout))
        {
            m_workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
        }

        // if its job is gas
        if (m_workerData.getWorkerJob(worker) == WorkerData::Gas)
        {
            const BWAPI::Unit refinery = m_workerData.getWorkerResource(worker);

            // if the refinery doesn't exist anymore
            if (!refinery || !refinery->exists() ||	refinery->getHitPoints() <= 0)
            {
                setMineralWorker(worker);
            }
        }
    }
}

void WorkerManager::setRepairWorker(BWAPI::Unit worker, BWAPI::Unit unitToRepair)
{
    m_workerData.setWorkerJob(worker, WorkerData::Repair, unitToRepair);
}

void WorkerManager::stopRepairing(BWAPI::Unit worker)
{
    m_workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
}

void WorkerManager::handleGasWorkers()
{
    if ((Config::Strategy::StrategyName == "7HatchSpeed_NoUpgrades" || Config::Strategy::StrategyName == "7HatchSpeed_Upgrades") && GasThreshold()==0) {
        Global::Workers().GasThreshold() += 100;
    }
    // for each unit we have
    for (auto & unit : BWAPI::Broodwar->self()->getUnits())
    {
        // if that unit is a refinery
        if (unit->getType().isRefinery() && unit->isCompleted() && !isGasStealRefinery(unit))
        {
            // get the number of workers currently assigned to it
            const int numAssigned = m_workerData.getNumAssignedWorkers(unit);
            int early_gascount_offset = 0;

            // if it's less than we want it to be, fill 'er up // changed for stopping gas at 100 for ling speed
            if (GasThreshold()==0 || GasThreshold()>BWAPI::Broodwar->self()->gatheredGas() || ((BWAPI::Broodwar->elapsedTime()>(12*60)) && Config::Strategy::StrategyName != "7HatchSpeed_NoUpgrades" && Config::Strategy::StrategyName != "7HatchSpeed_Upgrades") || BWAPI::Broodwar->elapsedTime() > (10 * 60))
            {
                //printf("\n\n%d,       %d\n\n",GasThreshold(),BWAPI::Broodwar->elapsedTime());
                if (Config::Strategy::StrategyName == "7HatchSpeed_NoUpgrades" && BWAPI::Broodwar->getFrameCount() / (24 * 60) < 11) {
                    //early_gascount_offset++;
                    //for now I dont want to integrate this, as it is causing too much gas strain
                }
                for (int i = 0; i < (Config::Macro::WorkersPerRefinery - numAssigned - early_gascount_offset); ++i)
                {
                    BWAPI::Unit gasWorker = getGasWorker(unit);
                    if (gasWorker)
                    {
                        m_workerData.setWorkerJob(gasWorker, WorkerData::Gas, unit);
                    }
                }
            }
            else 
            {
                for (auto& nestedunit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (nestedunit->getType().isWorker())
                    {
                        if (nestedunit->isGatheringGas())
                        {
                            setMineralWorker(nestedunit);
                        }
                    }
                }
            }
        }
    }

}

bool WorkerManager::isGasStealRefinery(BWAPI::Unit unit)
{
    auto * enemyBaseLocation = Global::Bases().getPlayerStartingBaseLocation(BWAPI::Broodwar->enemy());
    if (!enemyBaseLocation)
    {
        return false;
    }

    if (enemyBaseLocation->getGeysers().empty())
    {
        return false;
    }

    for (auto & u : enemyBaseLocation->getGeysers())
    {
        if (unit->getTilePosition() == u->getTilePosition())
        {
            return true;
        }
    }

    return false;
}

void WorkerManager::handleIdleWorkers()
{
    // for each of our workers
    for (auto & worker : m_workerData.getWorkers())
    {
        UAB_ASSERT(worker != nullptr, "Worker was null");

        // if it is idle
        if (m_workerData.getWorkerJob(worker) == WorkerData::Idle)
        {
            // send it to the nearest mineral patch
            setMineralWorker(worker);
        }
    }
}

void WorkerManager::handleRepairWorkers()
{
    if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
    {
        return;
    }

    for (auto & unit : BWAPI::Broodwar->self()->getUnits())
    {
        if (unit->getType().isBuilding() && (unit->getHitPoints() < unit->getType().maxHitPoints()))
        {
            const BWAPI::Unit repairWorker = getClosestMineralWorkerTo(unit);
            setRepairWorker(repairWorker, unit);
            break;
        }
    }
}

// bad micro for combat workers
void WorkerManager::handleCombatWorkers()
{
    for (auto & worker : m_workerData.getWorkers())
    {
        UAB_ASSERT(worker != nullptr, "Worker was null");

        if (m_workerData.getWorkerJob(worker) == WorkerData::Combat)
        {
            BWAPI::Broodwar->drawCircleMap(worker->getPosition().x, worker->getPosition().y, 4, BWAPI::Colors::Yellow, true);
            const BWAPI::Unit target = getClosestEnemyUnit(worker);

            if (target)
            {
                Micro::SmartAttackUnit(worker, target);
            }
        }
    }
}

BWAPI::Unit WorkerManager::getClosestEnemyUnit(BWAPI::Unit worker)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

    BWAPI::Unit closestUnit = nullptr;
    double closestDist = 10000;

    for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        const double dist = unit->getDistance(worker);

        if ((dist < 400) && (!closestUnit || (dist < closestDist)))
        {
            closestUnit = unit;
            closestDist = dist;
        }
    }

    return closestUnit;
}

void WorkerManager::finishedWithCombatWorkers()
{
    for (auto & worker : m_workerData.getWorkers())
    {
        UAB_ASSERT(worker != nullptr, "Worker was null");

        if (m_workerData.getWorkerJob(worker) == WorkerData::Combat)
        {
            setMineralWorker(worker);
        }
    }
}

BWAPI::Unit WorkerManager::getClosestMineralWorkerTo(BWAPI::Unit enemyUnit)
{
    UAB_ASSERT(enemyUnit != nullptr, "enemyUnit was null");

    BWAPI::Unit closestMineralWorker = nullptr;
    double closestDist = 100000;

    if (m_previousClosestWorker)
    {
        if (m_previousClosestWorker->getHitPoints() > 0)
        {
            return m_previousClosestWorker;
        }
    }

    // for each of our workers
    for (auto & worker : m_workerData.getWorkers())
    {
        UAB_ASSERT(worker != nullptr, "Worker was null");
        if (!worker)
        {
            continue;
        }
        // if it is a move worker
        if (m_workerData.getWorkerJob(worker) == WorkerData::Minerals)
        {
            double dist = worker->getDistance(enemyUnit);

            if (!closestMineralWorker || dist < closestDist)
            {
                closestMineralWorker = worker;
                closestDist = dist;
            }
        }
    }

    m_previousClosestWorker = closestMineralWorker;
    return closestMineralWorker;
}

BWAPI::Unit WorkerManager::getWorkerScout()
{
    // for each of our workers
    for (auto & worker : m_workerData.getWorkers())
    {
        UAB_ASSERT(worker != nullptr, "Worker was null");
        if (!worker)
        {
            continue;
        }
        // if it is a move worker
        if (m_workerData.getWorkerJob(worker) == WorkerData::Scout)
        {
            return worker;
        }
    }

    return nullptr;
}

void WorkerManager::handleMoveWorkers()
{
    // for each of our workers
    for (auto & worker : m_workerData.getWorkers())
    {
        UAB_ASSERT(worker != nullptr, "Worker was null");

        // if it is a move worker
        if (m_workerData.getWorkerJob(worker) == WorkerData::Move)
        {
            const WorkerMoveData data = m_workerData.getWorkerMoveData(worker);

            Micro::SmartMove(worker, data.position);
        }
    }
}

// set a worker to mine minerals
void WorkerManager::setMineralWorker(BWAPI::Unit unit)
{
    UAB_ASSERT(unit != nullptr, "Unit was null");

    // check if there is a mineral available to send the worker to
    BWAPI::Unit depot = getClosestDepot(unit);

    // if there is a valid mineral
    if (depot)
    {
        // update workerData with the new job
        m_workerData.setWorkerJob(unit, WorkerData::Minerals, depot);
    }
    else
    {
        // BWAPI::Broodwar->printf("No valid depot for mineral worker");
    }
}

BWAPI::Unit WorkerManager::getClosestDepot(BWAPI::Unit worker)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

    BWAPI::Unit closestDepot = nullptr;
    double closestDistance = 0;

    for (auto & unit : BWAPI::Broodwar->self()->getUnits())
    {
        UAB_ASSERT(unit != nullptr, "Unit was null");

        if (unit->getType().isResourceDepot() && (unit->isCompleted() || unit->getType() == BWAPI::UnitTypes::Zerg_Lair) && !m_workerData.depotIsFull(unit))
        {
            const double distance = unit->getDistance(worker);
            if (!closestDepot || distance < closestDistance)
            {
                closestDepot = unit;
                closestDistance = distance;
            }
        }
    }

    return closestDepot;
}


// other managers that need workers call this when they're done with a unit
void WorkerManager::finishedWithWorker(BWAPI::Unit unit)
{
    UAB_ASSERT(unit != nullptr, "Unit was null");

    //BWAPI::Broodwar->printf("BuildingManager finished with worker %d", unit->getID());
    if (m_workerData.getWorkerJob(unit) != WorkerData::Scout)
    {
        m_workerData.setWorkerJob(unit, WorkerData::Idle, nullptr);
    }
}

BWAPI::Unit WorkerManager::getGasWorker(BWAPI::Unit refinery)
{
    UAB_ASSERT(refinery != nullptr, "Refinery was null");

    BWAPI::Unit closestWorker = nullptr;
    double closestDistance = 0;

    for (auto & unit : m_workerData.getWorkers())
    {
        UAB_ASSERT(unit != nullptr, "Unit was null");

        if (m_workerData.getWorkerJob(unit) == WorkerData::Minerals)
        {
            const double distance = unit->getDistance(refinery);
            if (!closestWorker || distance < closestDistance)
            {
                closestWorker = unit;
                closestDistance = distance;
            }
        }
    }

    return closestWorker;
}

void WorkerManager::setBuildingWorker(BWAPI::Unit worker, Building & b)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

    m_workerData.setWorkerJob(worker, WorkerData::Build, b.type);
}

// gets a builder for BuildingManager to use
// if setJobAsBuilder is true (default), it will be flagged as a builder unit
// set 'setJobAsBuilder' to false if we just want to see which worker will build a building
BWAPI::Unit WorkerManager::getBuilder(Building & b, bool setJobAsBuilder)
{
    // variables to hold the closest worker of each type to the building
    BWAPI::Unit closestMovingWorker = nullptr;
    BWAPI::Unit closestMiningWorker = nullptr;
    double closestMovingWorkerDistance = 0;
    double closestMiningWorkerDistance = 0;

    // look through each worker that had moved there first
    for (auto & unit : m_workerData.getWorkers())
    {
        UAB_ASSERT(unit != nullptr, "Unit was null");

        // gas steal building uses scout worker
        if (b.isGasSteal && (m_workerData.getWorkerJob(unit) == WorkerData::Scout))
        {
            if (setJobAsBuilder)
            {
                m_workerData.setWorkerJob(unit, WorkerData::Build, b.type);
            }
            return unit;
        }

        // mining worker check
        if (unit->isCompleted() && (m_workerData.getWorkerJob(unit) == WorkerData::Minerals))
        {
            // if it is a new closest distance, set the pointer
            const double distance = unit->getDistance(BWAPI::Position(b.finalPosition));
            if (!closestMiningWorker || distance < closestMiningWorkerDistance)
            {
                closestMiningWorker = unit;
                closestMiningWorkerDistance = distance;
            }
        }

        // moving worker check
        if (unit->isCompleted() && (m_workerData.getWorkerJob(unit) == WorkerData::Move))
        {
            // if it is a new closest distance, set the pointer
            const double distance = unit->getDistance(BWAPI::Position(b.finalPosition));
            if (!closestMovingWorker || distance < closestMovingWorkerDistance)
            {
                closestMovingWorker = unit;
                closestMovingWorkerDistance = distance;
            }
        }
    }

    // if we found a moving worker, use it, otherwise using a mining worker
    BWAPI::Unit chosenWorker = closestMovingWorker ? closestMovingWorker : closestMiningWorker;

    // if the worker exists (one may not have been found in rare cases)
    if (chosenWorker && setJobAsBuilder)
    {
        m_workerData.setWorkerJob(chosenWorker, WorkerData::Build, b.type);
    }

    // return the worker
    return chosenWorker;
}

// sets a worker as a scout
void WorkerManager::setScoutWorker(BWAPI::Unit worker)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

    m_workerData.setWorkerJob(worker, WorkerData::Scout, nullptr);
}

// gets a worker which will move to a current location
BWAPI::Unit WorkerManager::getMoveWorker(BWAPI::Position p)
{
    // set up the pointer
    BWAPI::Unit closestWorker = nullptr;
    double closestDistance = 0;

    // for each worker we currently have
    for (auto & unit : m_workerData.getWorkers())
    {
        UAB_ASSERT(unit != nullptr, "Unit was null");

        // only consider it if it's a mineral worker
        if (unit->isCompleted() && m_workerData.getWorkerJob(unit) == WorkerData::Minerals)
        {
            // if it is a new closest distance, set the pointer
            const double distance = unit->getDistance(p);
            if (!closestWorker || distance < closestDistance)
            {
                closestWorker = unit;
                closestDistance = distance;
            }
        }
    }

    // return the worker
    return closestWorker;
}

// sets a worker to move to a given location
void WorkerManager::setMoveWorker(int mineralsNeeded, int gasNeeded, BWAPI::Position p)
{
    // set up the pointer
    BWAPI::Unit closestWorker = nullptr;
    double closestDistance = 0;

    // for each worker we currently have
    for (auto & unit : m_workerData.getWorkers())
    {
        UAB_ASSERT(unit != nullptr, "Unit was null");

        // only consider it if it's a mineral worker
        if (unit->isCompleted() && m_workerData.getWorkerJob(unit) == WorkerData::Minerals)
        {
            // if it is a new closest distance, set the pointer
            const double distance = unit->getDistance(p);
            if (!closestWorker || distance < closestDistance)
            {
                closestWorker = unit;
                closestDistance = distance;
            }
        }
    }

    if (closestWorker)
    {
        //BWAPI::Broodwar->printf("Setting worker job Move for worker %d", closestWorker->getID());
        m_workerData.setWorkerJob(closestWorker, WorkerData::Move, WorkerMoveData(mineralsNeeded, gasNeeded, p));
    }
    else
    {
        //BWAPI::Broodwar->printf("Error, no worker found");
    }
}

// will we have the required resources by the time a worker can travel a certain distance
bool WorkerManager::willHaveResources(int mineralsRequired, int gasRequired, double distance)
{
    // if we don't require anything, we will have it
    if (mineralsRequired <= 0 && gasRequired <= 0)
    {
        return true;
    }

    // the speed of the worker unit
    const double speed = BWAPI::Broodwar->self()->getRace().getWorker().topSpeed();

    UAB_ASSERT(speed > 0, "Speed is negative");

    // how many frames it will take us to move to the building location
    // add a second to account for worker getting stuck. better early than late
    const double framesToMove = (distance / speed) + 50;

    // magic numbers to predict income rates
    const double mineralRate = getNumMineralWorkers() * 0.045;
    const double gasRate     = getNumGasWorkers() * 0.07;

    // calculate if we will have enough by the time the worker gets there
    if (mineralRate * framesToMove >= mineralsRequired &&
        gasRate * framesToMove >= gasRequired)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void WorkerManager::setCombatWorker(BWAPI::Unit worker)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

    m_workerData.setWorkerJob(worker, WorkerData::Combat, nullptr);
}

void WorkerManager::onUnitMorph(BWAPI::Unit unit)
{
    UAB_ASSERT(unit != nullptr, "Unit was null");

    // if something morphs into a worker, add it
    if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self() && unit->getHitPoints() >= 0)
    {
        m_workerData.addWorker(unit);
    }

    // if something morphs into a building, it was a worker?
    if (unit->getType().isBuilding() && unit->getPlayer() == BWAPI::Broodwar->self() && unit->getPlayer()->getRace() == BWAPI::Races::Zerg)
    {
        //BWAPI::Broodwar->printf("A Drone started building");
        m_workerData.workerDestroyed(unit);
    }
}

void WorkerManager::onUnitShow(BWAPI::Unit unit)
{
    UAB_ASSERT(unit != nullptr, "Unit was null");

    // add the depot if it exists
    if (unit->getType().isResourceDepot() && unit->getPlayer() == BWAPI::Broodwar->self())
    {
        m_workerData.addDepot(unit);
    }

    // if something morphs into a worker, add it
    if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self() && unit->getHitPoints() >= 0)
    {
        //BWAPI::Broodwar->printf("A worker was shown %d", unit->getID());
        m_workerData.addWorker(unit);
    }
}


void WorkerManager::rebalanceWorkers()
{
    // for each worker
    for (auto & worker : m_workerData.getWorkers())
    {
        UAB_ASSERT(worker != nullptr, "Worker was null");

        if (!m_workerData.getWorkerJob(worker) == WorkerData::Minerals)
        {
            continue;
        }

        BWAPI::Unit depot = m_workerData.getWorkerDepot(worker);

        if (depot && m_workerData.depotIsFull(depot))
        {
            m_workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
        }
        else if (!depot)
        {
            m_workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
        }
    }
}

void WorkerManager::onUnitDestroy(BWAPI::Unit unit)
{
    UAB_ASSERT(unit != nullptr, "Unit was null");

    if (unit->getType().isResourceDepot() && unit->getPlayer() == BWAPI::Broodwar->self())
    {
        m_workerData.removeDepot(unit);
    }

    if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self())
    {
        m_workerData.workerDestroyed(unit);
    }

    if (unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field)
    {
        rebalanceWorkers();
    }
}

void WorkerManager::drawResourceDebugInfo()
{
    if (!Config::Debug::DrawResourceInfo)
    {
        return;
    }

    for (auto & worker : m_workerData.getWorkers())
    {
        UAB_ASSERT(worker != nullptr, "Worker was null");

        const char job = m_workerData.getJobCode(worker);

        const BWAPI::Position pos = worker->getTargetPosition();

        BWAPI::Broodwar->drawTextMap(worker->getPosition().x, worker->getPosition().y - 5, "\x07%c", job);

        BWAPI::Broodwar->drawLineMap(worker->getPosition().x, worker->getPosition().y, pos.x, pos.y, BWAPI::Colors::Cyan);

        const BWAPI::Unit depot = m_workerData.getWorkerDepot(worker);
        if (depot)
        {
            BWAPI::Broodwar->drawLineMap(worker->getPosition().x, worker->getPosition().y, depot->getPosition().x, depot->getPosition().y, BWAPI::Colors::Orange);
        }
    }
}

void WorkerManager::drawWorkerInformation(int x, int y)
{
    if (!Config::Debug::DrawWorkerInfo)
    {
        return;
    }

    BWAPI::Broodwar->drawTextScreen(x, y, "\x04 Workers %d", m_workerData.getNumMineralWorkers());
    BWAPI::Broodwar->drawTextScreen(x, y+20, "\x04 UnitID");
    BWAPI::Broodwar->drawTextScreen(x+50, y+20, "\x04 State");

    int yspace = 0;

    for (auto & unit : m_workerData.getWorkers())
    {
        UAB_ASSERT(unit != nullptr, "Worker was null");

        BWAPI::Broodwar->drawTextScreen(x, y+40+((yspace)*10), "\x03 %d", unit->getID());
        BWAPI::Broodwar->drawTextScreen(x+50, y+40+((yspace++)*10), "\x03 %c", m_workerData.getJobCode(unit));
    }
}

bool WorkerManager::isFree(BWAPI::Unit worker)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

    return m_workerData.getWorkerJob(worker) == WorkerData::Minerals || m_workerData.getWorkerJob(worker) == WorkerData::Idle;
}

bool WorkerManager::isWorkerScout(BWAPI::Unit worker)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

    return (m_workerData.getWorkerJob(worker) == WorkerData::Scout);
}

bool WorkerManager::isBuilder(BWAPI::Unit worker)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

    return (m_workerData.getWorkerJob(worker) == WorkerData::Build);
}

int WorkerManager::getNumMineralWorkers()
{
    return m_workerData.getNumMineralWorkers();
}

int WorkerManager::getNumIdleWorkers()
{
    return m_workerData.getNumIdleWorkers();
}

int WorkerManager::getNumGasWorkers()
{
    return m_workerData.getNumGasWorkers();
}
