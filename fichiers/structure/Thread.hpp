#include "fonctions.hpp"

#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>

struct MessageAvionToAPP {
    std::string avionCode;
    Coord currentPosition;
    int currentAltitude;
};

struct MessageAPPToAvion {
    std::string avionCode;
    std::vector<Coord> trajectory;
    int targetAltitude;
};

struct MessageAPPToTWR_DemandePiste {
    std::string avionCode;
};

struct MessageTWRToAPP_ReponsePiste {
    std::string avionCode;
    bool pisteLibre;
};

class DataHub {
public:
    std::queue<MessageAvionToAPP> app_in_queue;
    std::mutex app_in_mutex;
    std::condition_variable app_in_cv;

    // APP -> Avion
    std::map<std::string, std::queue<MessageAPPToAvion>> avion_out_queues;
    std::mutex avion_out_mutex;
    std::condition_variable avion_out_cv;

    // APP -> TWR
    std::queue<MessageAPPToTWR_DemandePiste> twr_request_queue;
    std::mutex twr_request_mutex;
    std::condition_variable twr_request_cv;

    // TWR -> APP
    std::queue<MessageTWRToAPP_ReponsePiste> twr_response_queue;
    std::mutex twr_response_mutex;
    std::condition_variable twr_response_cv;

    std::map<std::string, Coord> all_avion_positions; // Code avion -> Position
    std::mutex all_avion_positions_mutex;

    bool global_isPisteLibre = true;
    std::mutex global_pisteLibre_mutex;
    std::condition_variable global_pisteLibre_cv;
};

class ThreadedAvion : public Avion {
private:
    std::thread thread_;
    std::atomic<bool> stop_thread_;
    std::shared_ptr<DataHub> data; //protection au lieu de &

public:
    ThreadedAvion(const std::string& code_init, int alt_init, int vit_init, const Coord& pos_init, const std::string& dest_init, int parking_init, int carb_init, DataHub& sd);

    void run();
    void start();
    void stop();
    void join();

    void set_position(const Coord& pos);
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
};

class ThreadedAPP : public APP {
private:
    std::thread thread_;
    std::atomic<bool> stop_thread_;
    DataHub& shared_data_;
    std::string nom_aeroport_;

public:
    ThreadedAPP(const std::string& nom_aeroport, DataHub& sd);

    void run();
    void start();
    void stop();
    void join();
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