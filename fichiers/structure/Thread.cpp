#include "Thread.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <set>
#include <fstream>
#include <sstream>
#include "json.hpp"
#include <algorithm>
#include <iomanip>

using json = nlohmann::json;

// Fonction pour convertir coordonnées relatives
Coord convertirCoordonneesRelatives(float relX, float relY, int largeurFenetre, int hauteurFenetre) {
    int x = static_cast<int>(relX * largeurFenetre);
    int y = static_cast<int>(relY * hauteurFenetre);
    return Coord(x, y);
}

// ThreadedAvion - Version simplifiée
ThreadedAvion::ThreadedAvion(std::shared_ptr<DataHub> hub) : data(hub), heureDepart_(0) {}

void ThreadedAvion::run() {
    // Attendre l'heure de départ si spécifiée
    if (heureDepart_ > 0) {
        bool heure_atteinte = false;
        while (!stop_thread_ && !heure_atteinte) {
            std::time_t temps_actuel;
            {
                std::lock_guard<std::mutex> lock(data->temps_simulation_mutex);
                temps_actuel = data->temps_simulation;
            }

            if (temps_actuel >= heureDepart_) {
                heure_atteinte = true;
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    if (stop_thread_) return;

    // Décollage immédiat
    decollage();

    while (!stop_thread_) {
        avancer();

        // Mettre à jour la position partagée
        {
            std::lock_guard<std::mutex> lock(data->avions_positionsMutex);
            data->avions_positions[get_code()] = get_position();
        }

        // Gestion carburant
        carburant_et_urgences();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Nettoyer à la fin
    {
        std::lock_guard<std::mutex> lock(data->avions_positionsMutex);
        data->avions_positions.erase(get_code());
    }
}

void ThreadedAvion::start() {
    stop_thread_ = false;
    thread_ = std::thread(&ThreadedAvion::run, this);
}

void ThreadedAvion::stop() {
    stop_thread_ = true;
}

void ThreadedAvion::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void ThreadedAvion::Set_position(const Coord& pos) {
    set_position(pos);
}

void ThreadedAvion::carburant_et_urgences() {
    int consommation = (get_vitesse() / 100) + (get_altitude() / 1000);
    set_carburant(get_carburant() - consommation);

    if (get_carburant() < 200) {
        std::cout << "AVION " << get_code() << " CARBURANT FAIBLE: "
            << get_carburant() << std::endl;
    }

    if (get_carburant() <= 0) {
        std::cout << "AVION " << get_code() << " PLUS DE CARBURANT - CRASH!" << std::endl;
        stop_thread_ = true;
    }
}

void ThreadedAvion::avancer() {
    if (stop_thread_) return;

    Coord position_actuelle = get_position();
    Coord destination_actuelle = get_destination();

    int dx = destination_actuelle.get_x() - position_actuelle.get_x();
    int dy = destination_actuelle.get_y() - position_actuelle.get_y();
    float distance = std::sqrt(dx * dx + dy * dy);

    // Arriver à destination
    if (distance < 5) {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "   AVION " << get_code() << " ARRIVE A DESTINATION" << std::endl;
        std::cout << "   De " << aeroportDepart_ << " à " << aeroportDestination_ << std::endl;
        std::cout << "   Position finale: " << position_actuelle << std::endl;
        std::cout << std::string(50, '=') << std::endl;

        // Supprimer de la surveillance
        {
            std::lock_guard<std::mutex> lock(data->avions_positionsMutex);
            data->avions_positions.erase(get_code());
        }

        stop_thread_ = true;
        return;
    }

    if (distance > 2) {
        float vitesse = get_vitesse();

        // Évitement simple des collisions
        {
            std::lock_guard<std::mutex> lock(data->avions_positionsMutex);
            for (const auto& entry : data->avions_positions) {
                if (entry.first != get_code()) {
                    int dx_autre = entry.second.get_x() - position_actuelle.get_x();
                    int dy_autre = entry.second.get_y() - position_actuelle.get_y();
                    float distance_autre = std::sqrt(dx_autre * dx_autre + dy_autre * dy_autre);

                    if (distance_autre < 25.0) {
                        vitesse = vitesse * 0.4f;
                        float force_repulsion = 15.0f / distance_autre;
                        dx -= static_cast<int>((entry.second.get_x() - position_actuelle.get_x()) * force_repulsion);
                        dy -= static_cast<int>((entry.second.get_y() - position_actuelle.get_y()) * force_repulsion);
                    }
                }
            }
        }

        distance = std::sqrt(dx * dx + dy * dy);
        if (distance == 0) distance = 1;

        // Calcul mouvement
        float vitesse_ajustee = vitesse;
        if (distance < 100) {
            vitesse_ajustee = vitesse * (distance / 100.0f);
        }

        float ratio = (vitesse_ajustee * 0.03f) / distance;
        int moveX = static_cast<int>(dx * ratio);
        int moveY = static_cast<int>(dy * ratio);

        if (moveX == 0 && dx != 0) moveX = (dx > 0) ? 1 : -1;
        if (moveY == 0 && dy != 0) moveY = (dy > 0) ? 1 : -1;

        Coord nouvellePos(position_actuelle.get_x() + moveX,
            position_actuelle.get_y() + moveY);
        set_position(nouvellePos);
    }
}

void ThreadedAvion::decollage() {
    for (int i = 0; i < 11; i += 1) {
        set_vitesse(i * 100);
        set_altitude(i * 600);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    set_vitesse(1100);
}

// Setters pour le planning
void ThreadedAvion::setAeroportDepart(const std::string& aeroport) {
    aeroportDepart_ = aeroport;
}

void ThreadedAvion::setAeroportDestination(const std::string& aeroport) {
    aeroportDestination_ = aeroport;
}

void ThreadedAvion::setHeureDepart(std::time_t heure) {
    heureDepart_ = heure;
}

std::string ThreadedAvion::getAeroportDestination() const {
    return aeroportDestination_;
}

std::string ThreadedAvion::getAeroportDepart() const {
    return aeroportDepart_;
}

// ThreadedCCR - Version simplifiée
ThreadedCCR::ThreadedCCR(std::shared_ptr<DataHub> sd) : shared_data_(sd) {}

void ThreadedCCR::run() {
    // Démarrer l'heure de simulation
    {
        std::lock_guard<std::mutex> lock(shared_data_->temps_simulation_mutex);
        std::tm debut = {};
        debut.tm_hour = 0;
        debut.tm_min = 0;
        debut.tm_sec = 0;
        debut.tm_year = 2025 - 1900;
        debut.tm_mon = 0;
        debut.tm_mday = 1;
        shared_data_->temps_simulation = std::mktime(&debut);
    }

    // Thread pour faire avancer l'heure (15 minutes toutes les 2 secondes)
    temps_thread_ = std::thread([this]() {
        while (!stop_thread_) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            std::lock_guard<std::mutex> lock(shared_data_->temps_simulation_mutex);
            shared_data_->temps_simulation += 15 * 60; // 15 minutes
        }
        });

    while (!stop_thread_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (temps_thread_.joinable()) {
        temps_thread_.join();
    }
}

void ThreadedCCR::start() {
    stop_thread_ = false;
    thread_ = std::thread(&ThreadedCCR::run, this);
}

void ThreadedCCR::stop() {
    stop_thread_ = true;
}

void ThreadedCCR::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

// SimulationManager - Version simplifiée
void SimulationManager::addCCR() {
    auto ccr = std::make_unique<ThreadedCCR>(shared_data);
    ccrs.push_back(std::move(ccr));
}

void SimulationManager::addAPP(const std::string& airport_name) {
    // Pas besoin dans la version simplifiée
}

void SimulationManager::addTWR(int nb_places, const std::string& airport_name) {
    // Pas besoin dans la version simplifiée
}

void SimulationManager::startSimulation() {
    std::cout << "Demarrage simulation ATC..." << std::endl;

    for (auto& ccr : ccrs) ccr->start();
    for (auto& avion : avions) avion->start();
}

void SimulationManager::stopSimulation() {
    std::cout << "Arret simulation ATC..." << std::endl;

    for (auto& ccr : ccrs) ccr->stop();
    for (auto& avion : avions) avion->stop();

    for (auto& ccr : ccrs) ccr->join();
    for (auto& avion : avions) avion->join();
}

void SimulationManager::creerPlanning() {
    CCR ccr;
    ccr.planning();
    std::cout << "Planning cree avec planning.json" << std::endl;
    chargerPlanningDepuisFichier();
}

void SimulationManager::chargerPlanningDepuisFichier() {
    std::ifstream file("planning.json");
    if (!file.is_open()) {
        std::cerr << "Erreur: impossible d'ouvrir planning.json" << std::endl;
        return;
    }

    try {
        json planning_json;
        file >> planning_json;

        if (planning_json.contains("flights")) {
            std::lock_guard<std::mutex> lock(shared_data->planningMutex);
            shared_data->planning_vols.clear();

            for (const auto& flight : planning_json["flights"]) {
                VolPlanning vol;
                vol.jour = flight["day"];
                vol.depart = flight["departure"];
                vol.arrivee = flight["arrival"];
                vol.duree = flight["length"];
                vol.heure = flight["time"];

                if (flight.contains("code")) {
                    vol.codeVol = flight["code"];
                }
                else {
                    vol.codeVol = "AF" + std::to_string(1000 + shared_data->planning_vols.size());
                }

                shared_data->planning_vols.push_back(vol);
            }

            std::cout << "Planning charge: " << shared_data->planning_vols.size() << " vols" << std::endl;
        }
        file.close();
    }
    catch (const std::exception& e) {
        std::cerr << "Erreur lecture planning.json: " << e.what() << std::endl;
    }
}

void SimulationManager::creerVolsDepuisPlanning() {
    std::lock_guard<std::mutex> lock(shared_data->planningMutex);

    if (shared_data->planning_vols.empty()) {
        std::cout << "Aucun vol dans le planning" << std::endl;
        return;
    }

    std::cout << "\nCreation de vols depuis le planning..." << std::endl;

    std::map<std::string, Coord> positions_villes = {
        {"Paris", convertirCoordonneesRelatives(0.5f, 0.25f, 1920, 1080)},
        {"Nice", convertirCoordonneesRelatives(0.83f, 0.83f, 1920, 1080)},
        {"Lille", convertirCoordonneesRelatives(0.54f, 0.05f, 1920, 1080)},
        {"Strasbourg", convertirCoordonneesRelatives(0.84f, 0.27f, 1920, 1080)},
        {"Rennes", convertirCoordonneesRelatives(0.23f, 0.34f, 1920, 1080)},
        {"Toulouse", convertirCoordonneesRelatives(0.40f, 0.85f, 1920, 1080)}
    };

    int vols_crees = 0;
    const int MAX_VOLS = 10; // Limiter à 10 vols pour la démo

    for (size_t i = 0; i < shared_data->planning_vols.size() && vols_crees < MAX_VOLS; i++) {
        const auto& vol = shared_data->planning_vols[i];

        // Parser l'heure de départ
        int heures = 0, minutes = 0;
        if (sscanf(vol.heure.c_str(), "%dh%d", &heures, &minutes) < 1) {
            sscanf(vol.heure.c_str(), "%d", &heures);
        }

        if (heures >= 24) heures -= 24;

        std::tm heure_vol = {};
        heure_vol.tm_hour = heures;
        heure_vol.tm_min = minutes;
        heure_vol.tm_sec = 0;
        heure_vol.tm_year = 2025 - 1900;
        heure_vol.tm_mon = 0;
        heure_vol.tm_mday = 1;
        std::time_t temps_depart_vol = std::mktime(&heure_vol);

        std::string destination = vol.arrivee;

        if (vol.depart == destination ||
            positions_villes.find(vol.depart) == positions_villes.end() ||
            positions_villes.find(destination) == positions_villes.end()) {
            continue;
        }

        Coord pos_depart = positions_villes[vol.depart];
        Coord pos_arrivee = positions_villes[destination];

        auto avion = std::make_unique<ThreadedAvion>(shared_data);
        avion->set_code(vol.codeVol);
        avion->set_altitude(0);
        avion->set_vitesse(0);
        avion->set_position(pos_depart);
        avion->set_destination(pos_arrivee);
        avion->set_place_parking(0);
        avion->set_carburant(5000);
        avion->setAeroportDepart(vol.depart);
        avion->setAeroportDestination(destination);
        avion->setHeureDepart(temps_depart_vol);

        std::cout << "Vol " << vol.codeVol << ": " << vol.depart
            << " -> " << destination
            << " depart " << heures << "h" << std::setfill('0')
            << std::setw(2) << minutes << std::endl;

        avions.push_back(std::move(avion));
        vols_crees++;
    }

    std::cout << vols_crees << " vols programmes\n" << std::endl;
}

// Fonction pour formater l'heure
std::string formatTime(std::time_t t) {
    std::tm* timeinfo = std::localtime(&t);
    char buffer[10];
    std::strftime(buffer, sizeof(buffer), "%H:%M", timeinfo);
    return std::string(buffer);
}