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

using json = nlohmann::json;

Coord convertirCoordonneesRelatives(float relX, float relY, int largeurFenetre, int hauteurFenetre) {
    int x = static_cast<int>(relX * largeurFenetre);
    int y = static_cast<int>(relY * hauteurFenetre);
    return Coord(x, y);
}

ThreadedAvion::ThreadedAvion(std::shared_ptr<DataHub> hub)
    : data(hub), etat_(EN_VOL), tempsStationnement_(0), heureDepart_(0) {
}

void ThreadedAvion::run() {
    logJournal("CREATION", "Avion cree - Depart: " + aeroportDepart_ + " Destination: " + aeroportDestination_);

    bool heure_atteinte = false;
    while (!stop_thread_ && !heure_atteinte) {
        std::time_t temps_actuel;
        {
            std::lock_guard<std::mutex> lock(data->temps_simulation_mutex);
            temps_actuel = data->temps_simulation;
        }

        if (temps_actuel >= heureDepart_) {
            heure_atteinte = true;
            logJournal("DEBUT_VOL", "Heure de depart atteinte: " + formatTime(temps_actuel));
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    if (stop_thread_) return;

    decollage();

    while (!stop_thread_ && etat_ != TERMINE) {
        switch (etat_) {
        case EN_VOL:
        case APPROCHE_FINALE:
            avancer(); 

            {
                AvionToAPP mess;
                mess.avionCode = get_code();
                mess.Position = get_position();
                mess.Altitude = get_altitude();
                mess.etat = etat_;
                mess.urgence = (get_carburant() < 200);
                {
                    std::lock_guard<std::mutex> lock(data->avion_appMutex);
                    data->avion_appQueue.push(mess);
                }
                data->avion_appCondition.notify_one();
            }
            message_de_APP();
            carburant_et_urgences();
            break;

        case ATTERRISSAGE:
            atterrissage();
            break;

        case ROULAGE_ARRIVEE:
            roulage_arrivee();
            break;

        case STATIONNE:
            stationnement();
            break;

        case ROULAGE_DEPART:
            roulage_depart();
            break;

        case DECOLLAGE:
            decollage();
            setEtat(TERMINE);
            break;

        default:
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    {
        std::lock_guard<std::mutex> lock(data->avions_positionsMutex);
        data->avions_positions.erase(get_code());
        data->avions_etats.erase(get_code());
    }

    logJournal("TERMINE", "Vol termine");
}

void ThreadedAvion::logJournal(const std::string& action, const std::string& details) {
    JournalEntry entry;
    entry.timestamp = std::time(nullptr);
    entry.type = "AVION";
    entry.id = get_code();
    entry.action = action;
    entry.details = details;

    std::lock_guard<std::mutex> lock(data->journalMutex);
    data->journal.push_back(entry);
}

void ThreadedAvion::setEtat(EtatAvion etat) {
    etat_ = etat;
    std::lock_guard<std::mutex> lock(data->avions_positionsMutex);
    data->avions_etats[get_code()] = etat;
}

EtatAvion ThreadedAvion::getEtat() const {
    return etat_;
}

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

void ThreadedAvion::message_de_APP() {
    std::unique_lock<std::mutex> lock(data->app_avionMutex);

    auto it = data->app_avionQueue.find(get_code());
    if (it != data->app_avionQueue.end() && !it->second.empty()) {
        APPToAvion message = it->second.front();
        it->second.pop();
        lock.unlock();

        if (!message.trajectoire.empty()) {
            logJournal("TRAJECTOIRE_RECUE", "Nouvelle trajectoire de " +
                std::to_string(message.trajectoire.size()) + " points");
        }

        if (message.objectifAltitude > 0 && message.objectifAltitude < get_altitude()) {
            int diff = get_altitude() - message.objectifAltitude;
            set_altitude(get_altitude() - std::min(diff, 500));
            logJournal("DESCENTE", "Altitude: " + std::to_string(get_altitude()) + "ft");
        }
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

    if (get_carburant() < 200 && get_carburant() > 0) {
        static std::map<std::string, int> cooldown;
        if (cooldown[get_code()] <= 0) {
            logJournal("URGENCE", "CARBURANT FAIBLE: " + std::to_string(get_carburant()) + "L");

            AvionToAPP urgence;
            urgence.avionCode = get_code();
            urgence.Position = get_position();
            urgence.Altitude = get_altitude();
            urgence.etat = etat_;
            urgence.urgence = true;

            {
                std::lock_guard<std::mutex> lock(data->avion_appMutex);
                data->avion_appQueue.push(urgence);
            }
            data->avion_appCondition.notify_one();
            cooldown[get_code()] = 50;
        }
        cooldown[get_code()]--;
    }

    if (get_carburant() <= 0) {
        logJournal("CRASH", "PLUS DE CARBURANT");
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

    // SEUIL D'ARRIVÉE AUGMENTÉ pour éviter que l'avion tourne autour
    if (distance < 15.0f) {
        std::cout << "[" << get_code() << "] ARRIVE A DESTINATION - Distance: "
            << distance << "px - Passage en ATTERRISSAGE" << std::endl;
        setEtat(ATTERRISSAGE);
        logJournal("ARRIVEE_DESTINATION", "Entree en phase d'atterrissage - Distance: " +
            std::to_string(distance) + "px");
        return;
    }

    float vitesse_actuelle = static_cast<float>(get_vitesse());
    if (vitesse_actuelle <= 0) vitesse_actuelle = 100.0f;

    // Évitement de collisions
    {
        std::lock_guard<std::mutex> lock(data->avions_positionsMutex);
        for (const auto& entry : data->avions_positions) {
            if (entry.first != get_code()) {
                int dx_autre = entry.second.get_x() - position_actuelle.get_x();
                int dy_autre = entry.second.get_y() - position_actuelle.get_y();
                float dist_autre = std::sqrt(dx_autre * dx_autre + dy_autre * dy_autre);

                if (dist_autre < 50.0f && dist_autre > 0) {
                    vitesse_actuelle *= 0.5f;
                    dx -= static_cast<int>((dx_autre / dist_autre) * 10);
                    dy -= static_cast<int>((dy_autre / dist_autre) * 10);
                }
            }
        }
    }

    // Recalculer la distance après évitement
    distance = std::sqrt(dx * dx + dy * dy);
    if (distance == 0) distance = 1.0f;

    // Ralentissement progressif en approche finale
    float vitesse_ajustee = vitesse_actuelle;
    if (distance < 200.0f) {
        vitesse_ajustee = vitesse_actuelle * (distance / 200.0f);
        if (vitesse_ajustee < 50.0f) vitesse_ajustee = 50.0f;

        if (etat_ != APPROCHE_FINALE) {
            setEtat(APPROCHE_FINALE);
            std::cout << "[" << get_code() << "] APPROCHE FINALE - Distance: "
                << distance << "px" << std::endl;
        }
    }

    // Calcul du déplacement
    float ratio = (vitesse_ajustee * 0.05f) / distance; // Augmenté de 0.04 à 0.05
    int moveX = static_cast<int>(dx * ratio);
    int moveY = static_cast<int>(dy * ratio);

    // Assurer un déplacement minimum
    if (moveX == 0 && dx != 0) moveX = (dx > 0) ? 1 : -1;
    if (moveY == 0 && dy != 0) moveY = (dy > 0) ? 1 : -1;

    Coord nouvellePos(position_actuelle.get_x() + moveX, position_actuelle.get_y() + moveY);
    set_position(nouvellePos);

    // Mettre à jour dans shared_data
    {
        std::lock_guard<std::mutex> lock(data->avions_positionsMutex);
        data->avions_positions[get_code()] = nouvellePos;
    }
}
void ThreadedAvion::decollage() {
    int timing = 500;

    logJournal("DECOLLAGE_DEBUT", "Sequence de decollage");

    for (int i = 0; i < 11; i += 1) {
        set_vitesse(i * 100);
        set_altitude(i * 600);

        if (i < 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(timing));
        }
    }
    set_vitesse(1100);
    setEtat(EN_VOL);
    logJournal("DECOLLAGE_TERMINE", "Altitude croisiere: " + std::to_string(get_altitude()));
}

void ThreadedAvion::atterrissage() {
    logJournal("ATTERRISSAGE_DEBUT", "Sequence d'atterrissage");

    for (int i = 10; i >= 0; i--) {
        set_vitesse(i * 100);
        set_altitude(i * 600);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    setEtat(ROULAGE_ARRIVEE);
    logJournal("ATTERRISSAGE_TERMINE", "Sur la piste");
}

void ThreadedAvion::roulage_arrivee() {
    logJournal("ROULAGE_DEBUT", "Roulage vers parking");

    int tempsRoulage = 2000;
    std::this_thread::sleep_for(std::chrono::milliseconds(tempsRoulage));

    setEtat(STATIONNE);
    logJournal("ROULAGE_TERMINE", "Parking P" + std::to_string(get_place_parking()));
}

void ThreadedAvion::stationnement() {
    int duree = 10 + (rand() % 10);
    logJournal("STATIONNEMENT_DEBUT", "Duree: " + std::to_string(duree) + "s");

    std::this_thread::sleep_for(std::chrono::seconds(duree));

    TWRToAPP_DemandeDecollage demande;
    demande.avionCode = get_code();
    demande.placeParking = get_place_parking();

    {
        std::lock_guard<std::mutex> lock(data->twr_app_decolMutex);
        data->twr_app_decolQueue.push(demande);
    }
    data->twr_app_decolCondition.notify_one();

    logJournal("DEMANDE_DECOLLAGE", "Parking P" + std::to_string(get_place_parking()));

    setEtat(ROULAGE_DEPART);
}

void ThreadedAvion::roulage_depart() {
    int tempsRoulage = get_place_parking() * 500;
    logJournal("ROULAGE_DEPART_DEBUT", "Vers piste - Temps: " + std::to_string(tempsRoulage) + "ms");

    std::this_thread::sleep_for(std::chrono::milliseconds(tempsRoulage));

    std::unique_lock<std::mutex> lock(data->app_twr_decolMutex);
    data->app_twr_decolCondition.wait(lock, [this]() {
        std::queue<APPToTWR_AutorisationDecollage> temp = data->app_twr_decolQueue;
        while (!temp.empty()) {
            if (temp.front().avionCode == get_code() && temp.front().autorise) {
                return true;
            }
            temp.pop();
        }
        return false;
        });

    while (!data->app_twr_decolQueue.empty()) {
        auto auth = data->app_twr_decolQueue.front();
        data->app_twr_decolQueue.pop();
        if (auth.avionCode == get_code() && auth.autorise) {
            lock.unlock();
            setEtat(DECOLLAGE);
            logJournal("AUTORISATION_DECOLLAGE", "Autorise");
            return;
        }
    }
}

ThreadedCCR::ThreadedCCR(std::shared_ptr<DataHub> sd)
    : shared_data_(sd), temps_simulation_debut_(std::time(nullptr)) {
}

void ThreadedCCR::run() {
    JournalEntry entry;
    entry.timestamp = std::time(nullptr);
    entry.type = "CCR";
    entry.id = "CCR_NATIONAL";
    entry.action = "DEMARRAGE";
    entry.details = "Centre de Controle Regional demarre";
    {
        std::lock_guard<std::mutex> lock(shared_data_->journalMutex);
        shared_data_->journal.push_back(entry);
    }

    charger_planning();

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

    temps_thread_ = std::thread([this]() {
        while (!stop_thread_) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            std::lock_guard<std::mutex> lock(shared_data_->temps_simulation_mutex);
            shared_data_->temps_simulation += 15 * 60;
        }
        });

    while (!stop_thread_) {
        trafic_national();
        gerer_planning();
        reguler_flux();
        conflits();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
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

void ThreadedCCR::charger_planning() {
    std::lock_guard<std::mutex> lock(shared_data_->planningMutex);
    std::cout << "CCR: Planning de " << shared_data_->planning_vols.size() << " vols charge" << std::endl;
}

void ThreadedCCR::trafic_national() {
    static int compteur = 0;
    if (compteur % 20 == 0) {
        std::lock_guard<std::mutex> lock(shared_data_->avions_positionsMutex);

        JournalEntry entry;
        entry.timestamp = std::time(nullptr);
        entry.type = "CCR";
        entry.id = "CCR_NATIONAL";
        entry.action = "SURVEILLANCE";
        entry.details = "Nombre d'avions surveilles: " + std::to_string(shared_data_->avions_positions.size());

        {
            std::lock_guard<std::mutex> jlock(shared_data_->journalMutex);
            shared_data_->journal.push_back(entry);
        }
    }
    compteur++;
}

void ThreadedCCR::gerer_planning() {
    static int compteur_appel = 0;
    if (compteur_appel % 100 == 0) {
        JournalEntry entry;
        entry.timestamp = std::time(nullptr);
        entry.type = "CCR";
        entry.id = "CCR_NATIONAL";
        entry.action = "PLANNING_UPDATE";
        entry.details = "Planning aerien mis a jour";

        std::lock_guard<std::mutex> lock(shared_data_->journalMutex);
        shared_data_->journal.push_back(entry);
    }
    compteur_appel++;
}

void ThreadedCCR::reguler_flux() {
    std::lock_guard<std::mutex> lock(shared_data_->fluxMutex);

    for (auto& pair : shared_data_->flux_aeroports) {
        if (pair.second > 5) {
            JournalEntry entry;
            entry.timestamp = std::time(nullptr);
            entry.type = "CCR";
            entry.id = "CCR_NATIONAL";
            entry.action = "REGULATION_FLUX";
            entry.details = "Aeroport " + pair.first + " sature - " + std::to_string(pair.second) + " avions";

            std::lock_guard<std::mutex> jlock(shared_data_->journalMutex);
            shared_data_->journal.push_back(entry);
        }
    }
}

void ThreadedCCR::conflits() {
    std::lock_guard<std::mutex> lock(shared_data_->avions_positionsMutex);

    std::vector<std::pair<std::string, Coord>> avions_list;
    for (const auto& entry : shared_data_->avions_positions) {
        avions_list.push_back({ entry.first, entry.second });
    }

    for (size_t i = 0; i < avions_list.size(); i++) {
        for (size_t j = i + 1; j < avions_list.size(); j++) {
            int dx = avions_list[i].second.get_x() - avions_list[j].second.get_x();
            int dy = avions_list[i].second.get_y() - avions_list[j].second.get_y();
            float distance = std::sqrt(dx * dx + dy * dy);

            if (distance < 50) {
                JournalEntry entry;
                entry.timestamp = std::time(nullptr);
                entry.type = "CCR";
                entry.id = "CCR_NATIONAL";
                entry.action = "CONFLIT_DETECTE";
                entry.details = avions_list[i].first + " et " + avions_list[j].first +
                    " - Distance: " + std::to_string(distance) + "px";

                std::lock_guard<std::mutex> jlock(shared_data_->journalMutex);
                shared_data_->journal.push_back(entry);
            }
        }
    }
}

ThreadedAPP::ThreadedAPP(const std::string& nom_aeroport, Coord pos_aero, std::shared_ptr<DataHub> sd)
    : APP(), shared_data_(sd), nom_aeroport_(nom_aeroport), position_aeroport_(pos_aero) {
    JournalEntry entry;
    entry.timestamp = std::time(nullptr);
    entry.type = "APP";
    entry.id = "APP_" + nom_aeroport_;
    entry.action = "CREATION";
    entry.details = "APP cree a position " + std::to_string(pos_aero.get_x()) + "," + std::to_string(pos_aero.get_y());

    std::lock_guard<std::mutex> lock(shared_data_->journalMutex);
    shared_data_->journal.push_back(entry);
}

void ThreadedAPP::run() {
    while (!stop_thread_) {
        message_de_Avion();
        message_de_TWR();
        message_decollage_TWR();
        trafic_aerien();
        collisions();
        afficher_console();

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void ThreadedAPP::start() {
    stop_thread_ = false;
    thread_ = std::thread(&ThreadedAPP::run, this);
}

void ThreadedAPP::stop() {
    stop_thread_ = true;
}

void ThreadedAPP::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void ThreadedAPP::afficher_console() {
    static int compteur = 0;
    if (compteur % 50 == 0) {
        std::lock_guard<std::mutex> lock(shared_data_->avions_positionsMutex);

        std::cout << "\n=== APP " << nom_aeroport_ << " ===" << std::endl;
        int nb_avions = 0;
        for (const auto& entry : shared_data_->avions_positions) {
            int dx = entry.second.get_x() - position_aeroport_.get_x();
            int dy = entry.second.get_y() - position_aeroport_.get_y();
            float distance = std::sqrt(dx * dx + dy * dy);

            if (distance < 300) {
                std::cout << "  - " << entry.first << " distance: " << distance << "px" << std::endl;
                nb_avions++;
            }
        }
        std::cout << "Total: " << nb_avions << " avions en approche" << std::endl;
        std::cout << "===================" << std::endl;
    }
    compteur++;
}

void ThreadedAPP::message_de_Avion() {
    static std::set<std::string> avions_en_atterrissage;
    static std::set<std::string> avions_en_attente;

    std::unique_lock<std::mutex> lock(shared_data_->avion_appMutex);

    while (!shared_data_->avion_appQueue.empty()) {
        AvionToAPP message = shared_data_->avion_appQueue.front();
        shared_data_->avion_appQueue.pop();
        lock.unlock();

        {
            std::lock_guard<std::mutex> pos_lock(shared_data_->avions_positionsMutex);
            shared_data_->avions_positions[message.avionCode] = message.Position;
        }

        double distance = std::sqrt(
            std::pow(message.Position.get_x() - position_aeroport_.get_x(), 2) +
            std::pow(message.Position.get_y() - position_aeroport_.get_y(), 2)
        );

        const float SEUIL_APPROCHE = 300.0f;   // Zone d'approche
        const float SEUIL_FINAL = 100.0f;      // Approche finale
        const float SEUIL_ATTERRISSAGE = 30.0f; // Demande d'atterrissage (augmenté de 10 à 30)

        // Log pour debug
        if (distance < SEUIL_APPROCHE) {
            std::cout << "[APP " << nom_aeroport_ << "] " << message.avionCode
                << " - Distance: " << distance << "px";

            if (distance < SEUIL_ATTERRISSAGE) {
                std::cout << " - DEMANDE ATTERRISSAGE";
            }
            else if (distance < SEUIL_FINAL) {
                std::cout << " - APPROCHE FINALE";
            }
            else {
                std::cout << " - EN APPROCHE";
            }
            std::cout << std::endl;
        }

        // Demande d'atterrissage
        if (distance < SEUIL_ATTERRISSAGE) {
            if (avions_en_atterrissage.find(message.avionCode) == avions_en_atterrissage.end()) {
                std::cout << "[APP " << nom_aeroport_ << "] >>> DEMANDE PISTE pour "
                    << message.avionCode << " <<<" << std::endl;

                demande_piste_TWR(message.avionCode, message.urgence);
                avions_en_atterrissage.insert(message.avionCode);

                JournalEntry entry;
                entry.timestamp = std::time(nullptr);
                entry.type = "APP";
                entry.id = "APP_" + nom_aeroport_;
                entry.action = "DEMANDE_ATTERRISSAGE";
                entry.details = message.avionCode + " - Distance: " + std::to_string(distance);

                std::lock_guard<std::mutex> jlock(shared_data_->journalMutex);
                shared_data_->journal.push_back(entry);
            }
        }
        // Approche finale
        else if (distance < SEUIL_FINAL) {
            APPToAvion trajectoire;
            trajectoire.avionCode = message.avionCode;
            trajectoire.objectifAltitude = 1000;
            trajectoire.trajectoire.push_back(message.Position);
            trajectoire.trajectoire.push_back(position_aeroport_);

            {
                std::lock_guard<std::mutex> traj_lock(shared_data_->app_avionMutex);
                shared_data_->app_avionQueue[message.avionCode].push(trajectoire);
            }
            shared_data_->app_avionCondition.notify_one();
        }
        // Zone d'approche
        else if (distance < SEUIL_APPROCHE) {
            if (avions_en_attente.find(message.avionCode) == avions_en_attente.end()) {
                envoie_trajectoire_circulaire(message.avionCode, message.Position);
                avions_en_attente.insert(message.avionCode);
            }
        }

        lock.lock();
    }
}

void ThreadedAPP::message_de_TWR() {
    std::unique_lock<std::mutex> lock(shared_data_->twr_appMutex);

    while (!shared_data_->twr_appQueue.empty()) {
        TWRToAPP_ReponsePiste reponse = shared_data_->twr_appQueue.front();
        shared_data_->twr_appQueue.pop();
        lock.unlock();

        if (reponse.pisteLibre) {
            envoie_trajectoire_Avion(reponse.avionCode);

            JournalEntry entry;
            entry.timestamp = std::time(nullptr);
            entry.type = "APP";
            entry.id = "APP_" + nom_aeroport_;
            entry.action = "AUTORISATION_ATTERRISSAGE";
            entry.details = reponse.avionCode + " - Parking P" + std::to_string(reponse.placeParking);

            std::lock_guard<std::mutex> jlock(shared_data_->journalMutex);
            shared_data_->journal.push_back(entry);
        }
        else {
            std::lock_guard<std::mutex> pos_lock(shared_data_->avions_positionsMutex);
            if (shared_data_->avions_positions.find(reponse.avionCode) != shared_data_->avions_positions.end()) {
                envoie_trajectoire_circulaire(reponse.avionCode,
                    shared_data_->avions_positions[reponse.avionCode]);
            }
        }
        lock.lock();
    }
}

void ThreadedAPP::message_decollage_TWR() {
    std::unique_lock<std::mutex> lock(shared_data_->twr_app_decolMutex);

    while (!shared_data_->twr_app_decolQueue.empty()) {
        TWRToAPP_DemandeDecollage demande = shared_data_->twr_app_decolQueue.front();
        shared_data_->twr_app_decolQueue.pop();
        lock.unlock();

        autoriser_decollage(demande.avionCode);

        lock.lock();
    }
}

void ThreadedAPP::autoriser_decollage(const std::string& code) {
    APPToTWR_AutorisationDecollage auth;
    auth.avionCode = code;
    auth.autorise = true;

    {
        std::lock_guard<std::mutex> lock(shared_data_->app_twr_decolMutex);
        shared_data_->app_twr_decolQueue.push(auth);
    }
    shared_data_->app_twr_decolCondition.notify_one();

    JournalEntry entry;
    entry.timestamp = std::time(nullptr);
    entry.type = "APP";
    entry.id = "APP_" + nom_aeroport_;
    entry.action = "AUTORISATION_DECOLLAGE";
    entry.details = code;

    std::lock_guard<std::mutex> jlock(shared_data_->journalMutex);
    shared_data_->journal.push_back(entry);
}

void ThreadedAPP::envoie_trajectoire_Avion(const std::string& code) {
    APPToAvion message;
    message.avionCode = code;
    message.objectifAltitude = 500;
    message.trajectoire.push_back(position_aeroport_);
    {
        std::lock_guard<std::mutex> lock(shared_data_->app_avionMutex);
        shared_data_->app_avionQueue[code].push(message);
    }
    shared_data_->app_avionCondition.notify_one();
}

void ThreadedAPP::demande_piste_TWR(const std::string& code, bool urgence) {
    APPToTWR_DemandePiste demande;
    demande.avionCode = code;
    demande.urgence = urgence;

    {
        std::lock_guard<std::mutex> lock(shared_data_->app_twrMutex);
        shared_data_->app_twrQueue.push(demande);
    }
    shared_data_->app_twrCondition.notify_one();
}

void ThreadedAPP::envoie_trajectoire_circulaire(const std::string& code, const Coord& position_actuelle) {
    APPToAvion message;
    message.avionCode = code;
    message.objectifAltitude = 8000;

    int radius = 40;
    int points = 8;
    for (int i = 0; i < points; ++i) {
        double angle = 2 * 3.14 * i / points;
        int x = position_aeroport_.get_x() + static_cast<int>(radius * std::cos(angle));
        int y = position_aeroport_.get_y() + static_cast<int>(radius * std::sin(angle));
        message.trajectoire.push_back(Coord(x, y));
    }

    {
        std::lock_guard<std::mutex> lock(shared_data_->app_avionMutex);
        shared_data_->app_avionQueue[code].push(message);
    }
    shared_data_->app_avionCondition.notify_one();
}

void ThreadedAPP::trafic_aerien() {
    std::lock_guard<std::mutex> lock(shared_data_->avions_positionsMutex);

    int nb_avions = 0;
    for (const auto& entry : shared_data_->avions_positions) {
        int dx = entry.second.get_x() - position_aeroport_.get_x();
        int dy = entry.second.get_y() - position_aeroport_.get_y();
        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < 300) {
            nb_avions++;
        }
    }

    {
        std::lock_guard<std::mutex> flock(shared_data_->fluxMutex);
        shared_data_->flux_aeroports[nom_aeroport_] = nb_avions;
    }
}

void ThreadedAPP::collisions() {
    static std::set<std::string> avions_atterris;

    std::lock_guard<std::mutex> lock(shared_data_->avions_positionsMutex);

    std::vector<std::pair<std::string, Coord>> avions_en_vol;
    for (const auto& entry : shared_data_->avions_positions) {
        if (avions_atterris.find(entry.first) == avions_atterris.end()) {
            avions_en_vol.push_back(entry);
        }
    }

    for (size_t i = 0; i < avions_en_vol.size(); i++) {
        for (size_t j = i + 1; j < avions_en_vol.size(); j++) {
            const Coord& pos1 = avions_en_vol[i].second;
            const Coord& pos2 = avions_en_vol[j].second;

            int dx = pos1.get_x() - pos2.get_x();
            int dy = pos1.get_y() - pos2.get_y();
            float distance = std::sqrt(dx * dx + dy * dy);

            if (distance < 15) {
                JournalEntry entry;
                entry.timestamp = std::time(nullptr);
                entry.type = "APP";
                entry.id = "APP_" + nom_aeroport_;
                entry.action = "ALERTE_COLLISION";
                entry.details = avions_en_vol[i].first + " et " + avions_en_vol[j].first +
                    " - Distance: " + std::to_string(distance) + "px";

                std::lock_guard<std::mutex> jlock(shared_data_->journalMutex);
                shared_data_->journal.push_back(entry);
            }
        }
    }
}


ThreadedTWR::ThreadedTWR(int nb_places, const std::string& nom_a, std::shared_ptr<DataHub> sd)
    : TWR(nb_places), shared_data_(sd), nom_aeroport_(nom_a), parking_disponible_(nb_places, true) {
    JournalEntry entry;
    entry.timestamp = std::time(nullptr);
    entry.type = "TWR";
    entry.id = "TWR_" + nom_aeroport_;
    entry.action = "CREATION";
    entry.details = "TWR creee avec " + std::to_string(nb_places) + " places de parking";

    std::lock_guard<std::mutex> lock(shared_data_->journalMutex);
    shared_data_->journal.push_back(entry);
}

void ThreadedTWR::run() {
    while (!stop_thread_) {
        message_de_APP();
        gerer_parking();
        gerer_decollages();

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void ThreadedTWR::start() {
    stop_thread_ = false;
    thread_ = std::thread(&ThreadedTWR::run, this);
}

void ThreadedTWR::stop() {
    stop_thread_ = true;
}

void ThreadedTWR::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void ThreadedTWR::set_piste(bool facteur) {
    std::lock_guard<std::mutex> lock(shared_data_->global_pisteLibre_mutex);
    shared_data_->global_pisteLibre = facteur;

    if (facteur) {
        JournalEntry entry;
        entry.timestamp = std::time(nullptr);
        entry.type = "TWR";
        entry.id = "TWR_" + nom_aeroport_;
        entry.action = "PISTE_LIBEREE";
        entry.details = "Piste disponible";

        std::lock_guard<std::mutex> jlock(shared_data_->journalMutex);
        shared_data_->journal.push_back(entry);

        shared_data_->global_pisteLibreCondition.notify_all();
    }
}

int ThreadedTWR::attribuer_parking() {
    for (size_t i = 0; i < parking_disponible_.size(); i++) {
        if (parking_disponible_[i]) {
            parking_disponible_[i] = false;
            return i;
        }
    }
    return -1;
}

void ThreadedTWR::liberer_parking(int place) {
    if (place >= 0 && place < parking_disponible_.size()) {
        parking_disponible_[place] = true;

        JournalEntry entry;
        entry.timestamp = std::time(nullptr);
        entry.type = "TWR";
        entry.id = "TWR_" + nom_aeroport_;
        entry.action = "PARKING_LIBERE";
        entry.details = "Place P" + std::to_string(place) + " disponible";

        std::lock_guard<std::mutex> lock(shared_data_->journalMutex);
        shared_data_->journal.push_back(entry);
    }
}

void ThreadedTWR::ajouter_file_decollage(const std::string& code, int placeParking) {
    std::lock_guard<std::mutex> lock(file_decollage_mutex_);
    file_decollage_.push({ code, placeParking });
}

void ThreadedTWR::message_de_APP() {
    std::unique_lock<std::mutex> lock(shared_data_->app_twrMutex);

    while (!shared_data_->app_twrQueue.empty()) {
        APPToTWR_DemandePiste demande = shared_data_->app_twrQueue.front();
        shared_data_->app_twrQueue.pop();
        lock.unlock();

        std::unique_lock<std::mutex> piste_lock(shared_data_->global_pisteLibre_mutex);
        if (shared_data_->global_pisteLibre) {
            shared_data_->global_pisteLibre = false;
            piste_lock.unlock();

            int place = attribuer_parking();

            TWRToAPP_ReponsePiste reponse;
            reponse.avionCode = demande.avionCode;
            reponse.pisteLibre = true;
            reponse.placeParking = place;

            avions_parking_[demande.avionCode] = place;

            {
                std::lock_guard<std::mutex> lock_twr(shared_data_->twr_appMutex);
                shared_data_->twr_appQueue.push(reponse);
            }
            shared_data_->twr_appCondition.notify_one();

            JournalEntry entry;
            entry.timestamp = std::time(nullptr);
            entry.type = "TWR";
            entry.id = "TWR_" + nom_aeroport_;
            entry.action = "AUTORISATION_ATTERRISSAGE";
            entry.details = demande.avionCode + " - Parking P" + std::to_string(place);

            {
                std::lock_guard<std::mutex> jlock(shared_data_->journalMutex);
                shared_data_->journal.push_back(entry);
            }

            std::this_thread::sleep_for(std::chrono::seconds(3));

            set_piste(true);
        }
        else {
            piste_lock.unlock();

            TWRToAPP_ReponsePiste reponse;
            reponse.avionCode = demande.avionCode;
            reponse.pisteLibre = false;
            reponse.placeParking = -1;

            {
                std::lock_guard<std::mutex> lock_twr(shared_data_->twr_appMutex);
                shared_data_->twr_appQueue.push(reponse);
            }
            shared_data_->twr_appCondition.notify_one();
        }
        lock.lock();
    }
}

void ThreadedTWR::gerer_parking() {
    static int compteur = 0;
    if (compteur % 100 == 0) {
        int places_libres = 0;
        for (bool dispo : parking_disponible_) {
            if (dispo) places_libres++;
        }

        if (places_libres < parking_disponible_.size()) {
            JournalEntry entry;
            entry.timestamp = std::time(nullptr);
            entry.type = "TWR";
            entry.id = "TWR_" + nom_aeroport_;
            entry.action = "ETAT_PARKING";
            entry.details = std::to_string(places_libres) + "/" + std::to_string(parking_disponible_.size()) + " places libres";

            std::lock_guard<std::mutex> lock(shared_data_->journalMutex);
            shared_data_->journal.push_back(entry);
        }
    }
    compteur++;
}

void ThreadedTWR::gerer_decollages() {
    std::lock_guard<std::mutex> lock(file_decollage_mutex_);

    if (!file_decollage_.empty()) {
        std::vector<std::pair<std::string, int>> avions_tries;

        while (!file_decollage_.empty()) {
            avions_tries.push_back(file_decollage_.front());
            file_decollage_.pop();
        }

        std::sort(avions_tries.begin(), avions_tries.end(),
            [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
                return a.second > b.second;
            });

        if (!avions_tries.empty()) {
            std::unique_lock<std::mutex> piste_lock(shared_data_->global_pisteLibre_mutex);
            if (shared_data_->global_pisteLibre) {
                shared_data_->global_pisteLibre = false;
                piste_lock.unlock();

                std::string code_prioritaire = avions_tries[0].first;
                int place = avions_tries[0].second;

                JournalEntry entry;
                entry.timestamp = std::time(nullptr);
                entry.type = "TWR";
                entry.id = "TWR_" + nom_aeroport_;
                entry.action = "DECOLLAGE_PRIORITE";
                entry.details = code_prioritaire + " - Parking P" + std::to_string(place) + " (plus eloigne)";

                {
                    std::lock_guard<std::mutex> jlock(shared_data_->journalMutex);
                    shared_data_->journal.push_back(entry);
                }

                std::this_thread::sleep_for(std::chrono::seconds(2));

                liberer_parking(place);
                set_piste(true);

                for (size_t i = 1; i < avions_tries.size(); i++) {
                    file_decollage_.push(avions_tries[i]);
                }
            }
            else {
                for (const auto& avion : avions_tries) {
                    file_decollage_.push(avion);
                }
            }
        }
    }
}



std::string formatTime(std::time_t t) {
    std::tm* timeinfo = std::localtime(&t);
    char buffer[10];
    std::strftime(buffer, sizeof(buffer), "%H:%M", timeinfo);
    return std::string(buffer);
}


void SimulationManager::addCCR() {
    auto ccr = std::make_unique<ThreadedCCR>(shared_data);
    ccrs.push_back(std::move(ccr));
}

void SimulationManager::addAPP(const std::string& airport_name) {
    Coord position_aero;
    if (airport_name == "Paris")
        position_aero = convertirCoordonneesRelatives(0.5f, 0.25f, 1920, 1080);
    else if (airport_name == "Nice")
        position_aero = convertirCoordonneesRelatives(0.83f, 0.83f, 1920, 1080);
    else if (airport_name == "Lille")
        position_aero = convertirCoordonneesRelatives(0.54f, 0.05f, 1920, 1080);
    else if (airport_name == "Strasbourg")
        position_aero = convertirCoordonneesRelatives(0.84f, 0.27f, 1920, 1080);
    else if (airport_name == "Rennes")
        position_aero = convertirCoordonneesRelatives(0.23f, 0.34f, 1920, 1080);
    else if (airport_name == "Toulouse")
        position_aero = convertirCoordonneesRelatives(0.40f, 0.85f, 1920, 1080);
    else
        position_aero = convertirCoordonneesRelatives(0.5f, 0.25f, 1920, 1080);

    auto app = std::make_unique<ThreadedAPP>(airport_name, position_aero, shared_data);
    apps.push_back(std::move(app));
}

void SimulationManager::addTWR(int nb_places, const std::string& airport_name) {
    auto twr = std::make_unique<ThreadedTWR>(nb_places, airport_name, shared_data);
    twrs.push_back(std::move(twr));
}

void SimulationManager::startSimulation() {
    std::cout << "Demarrage simulation ATC..." << std::endl;

    for (auto& ccr : ccrs) ccr->start();
    for (auto& app : apps) app->start();
    for (auto& twr : twrs) twr->start();
}

void SimulationManager::stopSimulation() {
    std::cout << "Arret simulation ATC..." << std::endl;

    for (auto& ccr : ccrs) ccr->stop();
    for (auto& app : apps) app->stop();
    for (auto& twr : twrs) twr->stop();
    for (auto& avion : avions) avion->stop();

    for (auto& ccr : ccrs) ccr->join();
    for (auto& app : apps) app->join();
    for (auto& twr : twrs) twr->join();
    for (auto& avion : avions) avion->join();
}

void SimulationManager::sauvegarderJournal() {
    std::lock_guard<std::mutex> lock(shared_data->journalMutex);

    json journal_json = json::array();

    for (const auto& entry : shared_data->journal) {
        json j_entry;
        j_entry["timestamp"] = entry.timestamp;

        std::time_t t = entry.timestamp;
        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        j_entry["datetime"] = buffer;

        j_entry["type"] = entry.type;
        j_entry["id"] = entry.id;
        j_entry["action"] = entry.action;
        j_entry["details"] = entry.details;

        journal_json.push_back(j_entry);
    }

    std::ofstream file("journal.json");
    if (file.is_open()) {
        file << journal_json.dump(4);
        file.close();
        std::cout << "\nJournal sauvegarde dans journal.json ("
            << shared_data->journal.size() << " entrees)" << std::endl;
    }
    else {
        std::cerr << "Erreur: impossible de sauvegarder journal.json" << std::endl;
    }
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
    const int MAX_VOLS = 30; // Limiter à 30 vols pour ne pas surcharger

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

        // Vérifier que départ et destination sont différents
        if (vol.depart == destination) {
            continue;
        }

        // Vérifier que les villes existent
        if (positions_villes.find(vol.depart) == positions_villes.end()) {
            continue;
        }
        if (positions_villes.find(destination) == positions_villes.end()) {
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
        avion->set_carburant(3000); // Augmenté pour éviter les pénuries
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

    // Démarrer tous les avions
    std::cout << "Demarrage des threads avions..." << std::endl;
    for (auto& avion : avions) {
        avion->start();
    }
    std::cout << "Tous les avions sont demarres\n" << std::endl;
}