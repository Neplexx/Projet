#include "fonctions.hpp"

#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>
#include <memory>

struct AvionToAPP {
    std::string avionCode;
    Coord Position;
    int Altitude;
};

struct APPToAvion {
    std::string avionCode;
    std::vector<Coord> trajectoire;
    int objectifAltitude;
};

struct APPToTWR_DemandePiste {
    std::string avionCode;
};

struct TWRToAPP_ReponsePiste {
    std::string avionCode;
    bool pisteLibre;
};

class DataHub {
public:
    std::queue<AvionToAPP> avion_appQueue;
    std::mutex avion_appMutex;
    std::condition_variable avion_appCondition;

    // APP -> Avion
    std::map<std::string, std::queue<APPToAvion>> app_avionQueue;
    std::mutex app_avionMutex;
    std::condition_variable app_avionCondition;

    // APP -> TWR
    std::queue<APPToTWR_DemandePiste> app_twrQueue;
    std::mutex app_twrMutex;
    std::condition_variable app_twrCondition;

    // TWR -> APP
    std::queue<TWRToAPP_ReponsePiste> twr_appQueue;
    std::mutex twr_appMutex;
    std::condition_variable twr_appCondition;

    std::map<std::string, Coord> avions_positions; // Code avion -> Position
    std::mutex avions_positionsMutex;

    bool global_pisteLibre = true;
    std::mutex global_pisteLibre_mutex;
    std::condition_variable global_pisteLibreCondition;
};

class ThreadedAvion : public Avion {
private:
    std::thread thread_;
    std::atomic<bool> stop_thread_;
    std::shared_ptr<DataHub> data; //protection au lieu de &

public:
    ThreadedAvion(std::shared_ptr<DataHub> hub);
    
    void run();
    void start();
    void stop();
    void join();

    void Set_position(const Coord& pos);

    void message_de_APP();
    void carburant_et_urgences();
    void avancer();
};

class ThreadedCCR : public CCR {
private:
    std::thread thread_;
    std::atomic<bool> stop_thread_;
    DataHub& shared_data_;

public:
    ThreadedCCR(DataHub& sd);

    void run();
    void start();
    void stop();
    void join();

    void trafic_national();
    void gerer_planning();
    void conflits();
};

class ThreadedAPP : public APP {
private:
    std::thread thread_;
    std::atomic<bool> stop_thread_;
    DataHub& shared_data_;
    std::string nom_aeroport_;
    Coord position_aeroport_;

public:
    ThreadedAPP(const std::string& nom_aeroport, Coord position, DataHub& sd);

    void run();
    void start();
    void stop();
    void join();

    void message_de_Avion();
    void envoie_trajectoire_Avion(const std::string& code);
    void demande_piste_TWR(const std::string& code);
    void message_de_TWR();
    void trafic_aerien();
    void collisions();
};

class ThreadedTWR : public TWR {
private:
    std::thread thread_;
    std::atomic<bool> stop_thread_;
    DataHub& shared_data_;
    std::string nom_aeroport_;

public:
    ThreadedTWR(int nb_places, const std::string& nom_a, DataHub& sd);

    void run();
    void start();
    void stop();
    void join();

    void set_piste(bool facteur);
    void message_de_APP();
    void gerer_parking();
    void gerer_decollages();
};

class SimulationManager {
private:
    std::vector<ThreadedAvion> avions;
    std::vector<ThreadedCCR> ccrs;
    std::vector<ThreadedAPP> apps;
    std::vector<ThreadedTWR> twrs;
    DataHub shared_data;

public:
    SimulationManager();
     
    void addAvion(const std::string& code_init, int alt_init, int vit_init, const Coord& pos_init,const std::string& dest_init, int parking_init, int carb_init);
    void addCCR();
    void addAPP(const std::string& airport_name);
    void addTWR(int nb_places, const std::string& airport_name);

    void startSimulation();
    void stopSimulation();
};