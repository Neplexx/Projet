#pragma once
#include "fonctions.hpp"

#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>
#include <memory>
#include <set>
#include <ctime>
#include <string>

// Supprimer les états complexes, garder seulement l'essentiel
enum EtatAvion {
    EN_VOL,
    ATTERRISSAGE,
    TERMINE
};

// Structures simplifiées
struct VolPlanning {
    std::string jour;
    std::string depart;
    std::string arrivee;
    std::string duree;
    std::string heure;
    std::string codeVol;
};

// DataHub simplifié
class DataHub {
public:
    // Positions des avions
    std::map<std::string, Coord> avions_positions;
    std::mutex avions_positionsMutex;

    // Temps de simulation
    std::time_t temps_simulation;
    std::mutex temps_simulation_mutex;

    // Planning des vols
    std::vector<VolPlanning> planning_vols;
    std::mutex planningMutex;
};

// ThreadedAvion simplifié
class ThreadedAvion : public Avion {
private:
    std::thread thread_;
    std::atomic<bool> stop_thread_;
    std::shared_ptr<DataHub> data;
    std::string aeroportDepart_;
    std::string aeroportDestination_;
    std::time_t heureDepart_;

public:
    ThreadedAvion(std::shared_ptr<DataHub> hub);

    void run();
    void start();
    void stop();
    void join();

    void Set_position(const Coord& pos);

    // Fonctions pour le planning
    void setAeroportDepart(const std::string& aeroport);
    void setAeroportDestination(const std::string& aeroport);
    void setHeureDepart(std::time_t heure);
    std::string getAeroportDestination() const;
    std::string getAeroportDepart() const;

    // Fonctions de vol simplifiées
    void carburant_et_urgences();
    void avancer();
    void decollage();
};

// ThreadedCCR simplifié (pour gérer l'heure)
class ThreadedCCR {
private:
    std::thread thread_;
    std::thread temps_thread_;
    std::atomic<bool> stop_thread_;
    std::shared_ptr<DataHub> shared_data_;
    std::time_t temps_simulation_debut_;

public:
    ThreadedCCR(std::shared_ptr<DataHub> sd);

    void run();
    void start();
    void stop();
    void join();
};

// SimulationManager simplifié
class SimulationManager {
public:
    std::vector<std::unique_ptr<ThreadedAvion>> avions;
    std::shared_ptr<DataHub> shared_data;

private:
    std::vector<std::unique_ptr<ThreadedCCR>> ccrs;

public:
    SimulationManager() : shared_data(std::make_shared<DataHub>()) {}

    void addCCR();
    void addAPP(const std::string& airport_name);  // Gardé pour compatibilité mais vide
    void addTWR(int nb_places, const std::string& airport_name);  // Gardé pour compatibilité mais vide

    void startSimulation();
    void stopSimulation();
    void creerPlanning();
    void chargerPlanningDepuisFichier();
    void creerVolsDepuisPlanning();
};

// Fonctions utilitaires
Coord convertirCoordonneesRelatives(float relX, float relY, int largeurFenetre, int hauteurFenetre);
std::string formatTime(std::time_t t);