#pragma once

#include "Common.h"

namespace UAlbertaBot
{
class ScoutManager
{
    friend class Global;

    std::string		m_scoutStatus       = "None";
    std::string		m_gasStealStatus    = "None";
    int				m_numWorkerScouts   = 0;
    int				m_previousScoutHP   = 0;
    bool			m_scoutUnderAttack  = false;
    bool			m_didGasSteal       = false;
    bool			m_gasStealFinished  = false;

    bool enemyWorkerInRadius();
    bool immediateThreat();
    void gasSteal();
    void fleeScout();
    void drawScoutInformation(int x, int y);
    BWAPI::Position	getFleePosition();
    BWAPI::Unit		getEnemyGeyser();
    BWAPI::Unit		closestEnemyWorker();

    ScoutManager();

public:
    BWAPI::Unit		m_workerScout = nullptr;
    void update();
    void moveScouts();
    void releaseScout(BWAPI::Unit workerScoutUnit);
    void setWorkerScout(BWAPI::Unit unit);
};
}