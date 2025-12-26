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

enum EtatAvion {
    EN_VOL,
    APPROCHE_FINALE,
    ATTERRISSAGE,
    ROULAGE_ARRIVEE,
    STATIONNE,
    ROULAGE_DEPART,
    DECOLLAGE,
    TERMINE
};

struct AvionToAPP {
    std::string avionCode;
    Coord Position;
    int Altitude;
    EtatAvion etat;
    bool urgence;
};

struct APPToAvion {
    std::string avionCode;
    std::vector<Coord> trajectoire;
    int objectifAltitude;
};

struct APPToTWR_DemandePiste {
    std::string avionCode;
    bool urgence;
};

struct TWRToAPP_ReponsePiste {
    std::string avionCode;
    bool pisteLibre;
    int placeParking;
};

struct TWRToAPP_DemandeDecollage {
    std::string avionCode;
    int placeParking;
};

struct APPToTWR_AutorisationDecollage {
    std::string avionCode;
    bool autorise;
};

struct VolPlanning {
    std::string jour;
    std::string depart;
    std::string arrivee;
    std::string duree;
    std::string heure;
    std::string codeVol;
};

struct JournalEntry {
    std::time_t timestamp;
    std::string type;
    std::string id;
    std::string action;
    std::string details;
};

class DataHub {
public:
    std::queue<AvionToAPP> avion_appQueue;
    std::mutex avion_appMutex;
    std::condition_variable avion_appCondition;

    std::map<std::string, std::queue<APPToAvion>> app_avionQueue;
    std::mutex app_avionMutex;
    std::condition_variable app_avionCondition;

    std::queue<APPToTWR_DemandePiste> app_twrQueue;
    std::mutex app_twrMutex;
    std::condition_variable app_twrCondition;

    std::queue<TWRToAPP_ReponsePiste> twr_appQueue;
    std::mutex twr_appMutex;
    std::condition_variable twr_appCondition;

    std::queue<TWRToAPP_DemandeDecollage> twr_app_decolQueue;
    std::mutex twr_app_decolMutex;
    std::condition_variable twr_app_decolCondition;

    std::queue<APPToTWR_AutorisationDecollage> app_twr_decolQueue;
    std::mutex app_twr_decolMutex;
    std::condition_variable app_twr_decolCondition;

    std::map<std::string, Coord> avions_positions;
    std::map<std::string, EtatAvion> avions_etats;
    std::map<std::string, std::string> avions_aeroports;
    std::mutex avions_positionsMutex;

    bool global_pisteLibre = true;
    std::mutex global_pisteLibre_mutex;
    std::condition_variable global_pisteLibreCondition;

    std::vector<JournalEntry> journal;
    std::mutex journalMutex;

    std::vector<VolPlanning> planning_vols;
    std::mutex planningMutex;

    std::map<std::string, int> flux_aeroports;
    std::mutex fluxMutex;

    std::time_t temps_simulation;
    std::mutex temps_simulation_mutex;
    int multiplicateur_temps = 450; // 15 min en 2s = x450
};

class ThreadedAvion : public Avion {
private:
    std::thread thread_;
    std::atomic<bool> stop_thread_;
    std::shared_ptr<DataHub> data;
    EtatAvion etat_;
    std::string aeroportDepart_;
    std::string aeroportDestination_;
    int tempsStationnement_;
    std::time_t heureDepart_;

public:
    ThreadedAvion(std::shared_ptr<DataHub> hub);

    void run();
    void start();
    void stop();
    void join();

    void Set_position(const Coord& pos);
    void setEtat(EtatAvion etat);
    EtatAvion getEtat() const;
    void setAeroportDepart(const std::string& aeroport);
    void setAeroportDestination(const std::string& aeroport);
    void setHeureDepart(std::time_t heure);
    std::string getAeroportDestination() const;

    void message_de_APP();
    void carburant_et_urgences();
    void avancer();
    void decollage();
    void atterrissage();
    void roulage_arrivee();
    void stationnement();
    void roulage_depart();
    void logJournal(const std::string& action, const std::string& details);

    std::string getAeroportDepart() const;
};

class ThreadedCCR : public CCR {
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

    void trafic_national();
    void gerer_planning();
    void conflits();
    void reguler_flux();
    void charger_planning();
};

class ThreadedAPP : public APP {
private:
    std::thread thread_;
    std::atomic<bool> stop_thread_;
    std::shared_ptr<DataHub> shared_data_;
    std::string nom_aeroport_;
    Coord position_aeroport_;
    void envoie_trajectoire_circulaire(const std::string& code, const Coord& position_actuelle);

public:
    ThreadedAPP(const std::string& nom_aeroport, Coord position, std::shared_ptr<DataHub> sd);

    void run();
    void start();
    void stop();
    void join();

    void message_de_Avion();
    void envoie_trajectoire_Avion(const std::string& code);
    void demande_piste_TWR(const std::string& code, bool urgence);
    void message_de_TWR();
    void message_decollage_TWR();
    void autoriser_decollage(const std::string& code);
    void trafic_aerien();
    void collisions();
    void afficher_console();
};

class ThreadedTWR : public TWR {
private:
    std::thread thread_;
    std::atomic<bool> stop_thread_;
    std::shared_ptr<DataHub> shared_data_;
    std::string nom_aeroport_;
    std::queue<std::pair<std::string, int>> file_decollage_;
    std::mutex file_decollage_mutex_;
    std::vector<bool> parking_disponible_;
    std::map<std::string, int> avions_parking_;

public:
    ThreadedTWR(int nb_places, const std::string& nom_a, std::shared_ptr<DataHub> sd);

    void run();
    void start();
    void stop();
    void join();

    void set_piste(bool facteur);
    void message_de_APP();
    void gerer_parking();
    void gerer_decollages();
    void ajouter_file_decollage(const std::string& code, int placeParking);
    int attribuer_parking();
    void liberer_parking(int place);
};

class SimulationManager {
public:
    std::vector<std::unique_ptr<ThreadedAvion>> avions;
    std::shared_ptr<DataHub> shared_data;

private:
    std::vector<std::unique_ptr<ThreadedCCR>> ccrs;
    std::vector<std::unique_ptr<ThreadedAPP>> apps;
    std::vector<std::unique_ptr<ThreadedTWR>> twrs;

public:
    SimulationManager() : shared_data(std::make_shared<DataHub>()) {}

    void addAvion(const std::string& code_init, int alt_init, int vit_init,
        const Coord& pos_init, const std::string& dest_init,
        int parking_init, int carb_init);

    void addCCR();
    void addAPP(const std::string& airport_name);
    void addTWR(int nb_places, const std::string& airport_name);

    void startSimulation();
    void stopSimulation();
    void sauvegarderJournal();
    void chargerPlanningDepuisFichier();
    void creerVolsDepuisPlanning();

    void creerPlanning();
};

Coord convertirCoordonneesRelatives(float relX, float relY, int largeurFenetre, int hauteurFenetre);
std::string getAeroportFromCoord(const Coord& coord);
std::string formatTime(std::time_t t);