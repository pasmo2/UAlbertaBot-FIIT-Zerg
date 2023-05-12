#pragma once

#include <Common.h>
#include "WorkerData.h"

namespace UAlbertaBot
{
class Building;
//extern int gasThreshold;

class OverlordInfo
{
public:
    BWAPI::Unit unit_identification;
    std::string role;
    BWAPI::Position assigned_base;
};

class WorkerManager
{
    friend class Global;
    int _gasThreshold=0;
    int _numOfMacroHatcheries = 0;
    int _initialScout = 0;
    WorkerData  m_workerData;
    BWAPI::Unit m_previousClosestWorker = nullptr;

    bool isGasStealRefinery(BWAPI::Unit unit);


    void createMacroHatch();
    void handleIdleWorkers();
    void handleGasWorkers();
    void handleGasStopping(int threshold);
    void handleGasStopping();
    void handleMoveWorkers();
    void handleCombatWorkers();
    void handleRepairWorkers();
    int overlord_handling_call_count = 0;
    WorkerManager();

    //void increaseGasThreshold(int amount);

public:

    std::vector<OverlordInfo> overlords_vector;
    int& GasThreshold() { return _gasThreshold; };
    int const& GasThreshold() const { return _gasThreshold; };
    int& numOfMacroHatcheries() { return _numOfMacroHatcheries; };
    int const& numOfMacroHatcheries() const { return _numOfMacroHatcheries; };
    int& initialScout() { return _initialScout; };
    int const& initialScout() const { return _initialScout; };

    void onFrame();
    void QueueWandererMovement(BWAPI::Unit wanderer);
    void instantScout();
    void overlordHandling();
    bool tileHasUnitNearby(BWAPI::TilePosition pos);
    void createSpine();
    //int gasThreshold;
    //void increaseGasThreshold();
    void onUnitDestroy(BWAPI::Unit unit);
    void onUnitMorph(BWAPI::Unit unit);
    void onUnitShow(BWAPI::Unit unit);
    void finishedWithWorker(BWAPI::Unit unit);
    void finishedWithCombatWorkers();
    void drawResourceDebugInfo();
    void updateWorkerStatus();
    void drawWorkerInformation(int x, int y);
    void setMineralWorker(BWAPI::Unit unit);


    int  getNumMineralWorkers();
    int  getNumGasWorkers();
    int  getNumIdleWorkers();
    void setScoutWorker(BWAPI::Unit worker);

    bool isWorkerScout(BWAPI::Unit worker);
    bool isFree(BWAPI::Unit worker);
    bool isBuilder(BWAPI::Unit worker);

    BWAPI::Unit getBuilder(Building & b, bool setJobAsBuilder = true);
    BWAPI::Unit getMoveWorker(BWAPI::Position p);
    BWAPI::Unit getClosestDepot(BWAPI::Unit worker);
    BWAPI::Unit getGasWorker(BWAPI::Unit refinery);
    BWAPI::Unit getClosestEnemyUnit(BWAPI::Unit worker);
    BWAPI::Unit getClosestMineralWorkerTo(BWAPI::Unit enemyUnit);
    BWAPI::Unit getWorkerScout();

    void setBuildingWorker(BWAPI::Unit worker, Building & b);
    void setRepairWorker(BWAPI::Unit worker, BWAPI::Unit unitToRepair);
    void stopRepairing(BWAPI::Unit worker);
    void setMoveWorker(int m, int g, BWAPI::Position p);
    void setCombatWorker(BWAPI::Unit worker);

    bool willHaveResources(int mineralsRequired, int gasRequired, double distance);
    void rebalanceWorkers();
};
}