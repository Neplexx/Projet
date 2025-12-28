#include "Thread.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <set>

Coord convertirCoordonneesRelatives(float relX, float relY, int largeurFenetre, int hauteurFenetre) {
    int x = static_cast<int>(relX * largeurFenetre);
    int y = static_cast<int>(relY * hauteurFenetre);
    return Coord(x, y);
}

std::string determinerAeroportFromCoord(const Coord& destination, std::shared_ptr<DataHub> data) {
    std::lock_guard<std::mutex> lock(data->coordonneesAeroportsMutex);

    const int TOLERANCE = 50;

    for (const auto& pair : data->coordonneesAeroports) {
        const Coord& coord = pair.second;
        if (std::abs(destination.get_x() - coord.get_x()) <= TOLERANCE &&
            std::abs(destination.get_y() - coord.get_y()) <= TOLERANCE) {
            return pair.first;
        }
    }

    std::cerr << "ERREUR: Aucun aéroport trouvé pour coordonnées "
        << destination << std::endl;
    std::cerr << "Aéroports disponibles:" << std::endl;
    for (const auto& pair : data->coordonneesAeroports) {
        std::cerr << "  - " << pair.first << ": " << pair.second << std::endl;
    }

    return "Inconnu";
}



ThreadedAvion::ThreadedAvion(std::shared_ptr<DataHub> hub) : data(hub), a_atterri_(false) {}

void ThreadedAvion::run() {
    decollage();

    while (!stop_thread_ && !a_atterri_) {
        avancer();

        if (!stop_thread_ && !a_atterri_) {
            AvionToAPP mess;
            mess.avionCode = get_code();
            mess.Position = get_position();
            mess.Altitude = get_altitude();
            {
                std::lock_guard<std::mutex> lock(data->avion_appMutex);
                data->avion_appQueue.push(mess);
            }
            data->avion_appCondition.notify_one();

            message_de_APP();
            carburant_et_urgences();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[FIN] Thread avion " << get_code() << " terminé." << std::endl;
}

void ThreadedAvion::message_de_APP() {
    std::unique_lock<std::mutex> lock(data->app_avionMutex);

    auto it = data->app_avionQueue.find(get_code());
    if (it != data->app_avionQueue.end() && !it->second.empty()) {
        APPToAvion message = it->second.front();
        it->second.pop();
        lock.unlock();

        if (!message.trajectoire.empty()) {
            std::cout << "Avion " << get_code() << " reçoit nouvelle trajectoire de "
                << message.trajectoire.size() << " points" << std::endl;
        }

        if (message.objectifAltitude > 0) {
            std::cout << "Avion " << get_code() << " objectif altitude: "
                << message.objectifAltitude << std::endl;
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

    if (get_carburant() < 200) {
        std::cout << "AVION " << get_code() << " CARBURANT FAIBLE: "
            << get_carburant() << "L" << std::endl;

        AvionToAPP urgence;
        urgence.avionCode = get_code();
        urgence.Position = get_position();
        urgence.Altitude = get_altitude();

        {
            std::lock_guard<std::mutex> lock(data->avion_appMutex);
            data->avion_appQueue.push(urgence);
        }
        data->avion_appCondition.notify_one();
    }
    if (get_carburant() <= 0) {
        std::cout << "AVION " << get_code() << " PLUS DE CARBURANT - CRASH!" << std::endl;
        stop_thread_ = true;
    }
}

void ThreadedAvion::avancer() {
    if (stop_thread_ || a_atterri_) {
        return;
    }

    Coord position_actuelle = get_position();
    Coord destination_actuelle = get_destination();

    static std::map<std::string, bool> first_log;
    if (!first_log[get_code()]) {
        std::cout << "\n[DEMARRAGE] Avion " << get_code() << std::endl;
        std::cout << "  Position départ: " << position_actuelle << std::endl;
        std::cout << "  Destination: " << destination_actuelle << std::endl;

        std::string aeroport_dest = determinerAeroportFromCoord(destination_actuelle, data);
        std::cout << "  Aéroport destination: " << aeroport_dest << std::endl;
        first_log[get_code()] = true;
    }

    int dx = destination_actuelle.get_x() - position_actuelle.get_x();
    int dy = destination_actuelle.get_y() - position_actuelle.get_y();
    float distance = std::sqrt(dx * dx + dy * dy);

    std::string aeroport_destination = determinerAeroportFromCoord(destination_actuelle, data);

    if (distance < 5) {
        {
            std::lock_guard<std::mutex> lock(data->avionsAtterrisMutex);
            if (data->avionsAtterris.find(get_code()) != data->avionsAtterris.end()) {
                return;
            }
            data->avionsAtterris.insert(get_code());
        }

        a_atterri_ = true;
        set_altitude(0);
        set_vitesse(0);

        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "   AVION " << get_code() << " ATTERRIT A " << aeroport_destination << std::endl;
        std::cout << "   Position finale: " << position_actuelle << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        {
            std::lock_guard<std::mutex> lock(data->parkingsMutex);

            if (data->parkingsLibres.find(aeroport_destination) == data->parkingsLibres.end()) {
                std::cerr << "ERREUR: Aéroport " << aeroport_destination << " non trouvé!" << std::endl;
                supprimerDuSysteme();
                return;
            }

            auto& parkings = data->parkingsLibres[aeroport_destination];
            int placeLibre = -1;

            for (size_t i = 0; i < parkings.size(); i++) {
                if (parkings[i]) {
                    placeLibre = i;
                    break;
                }
            }

            if (placeLibre != -1) {
                parkings[placeLibre] = false;

                AvionAuParking avionParking;
                avionParking.code = get_code();
                avionParking.aeroport = aeroport_destination;
                avionParking.numeroPlace = placeLibre;
                avionParking.heureArrivee = std::chrono::steady_clock::now();
                avionParking.tempsParkingSecondes = 10;

                data->avionsAuParking[aeroport_destination].push_back(avionParking);

                std::cout << "[PARKING] Avion " << get_code() << " garé au parking P"
                    << (placeLibre + 1) << " de " << aeroport_destination
                    << " pour " << avionParking.tempsParkingSecondes << "s" << std::endl;
            }
            else {
                std::cout << "[WARNING] Pas de place disponible à " << aeroport_destination
                    << " pour " << get_code() << std::endl;
            }
        }
        {
            std::lock_guard<std::mutex> lock(data->avions_positionsMutex);
            data->avions_positions.erase(get_code());
            std::cout << "[CARTE] Avion " << get_code()
                << " retiré de la carte internationale" << std::endl;
        }
        {
            std::lock_guard<std::mutex> lock(data->app_avionMutex);
            data->app_avionQueue.erase(get_code());
        }

        {
            std::lock_guard<std::mutex> lock(data->avion_appMutex);
            std::queue<AvionToAPP> temp_queue;
            while (!data->avion_appQueue.empty()) {
                AvionToAPP msg = data->avion_appQueue.front();
                data->avion_appQueue.pop();
                if (msg.avionCode != get_code()) {
                    temp_queue.push(msg);
                }
            }
            data->avion_appQueue = std::move(temp_queue);
        }
        stop_thread_ = true;
        return;
    }

    if (distance > 2) {
        float vitesse = get_vitesse();
        static std::map<std::string, int> warnings_cooldown;

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

                        if (warnings_cooldown[entry.first] <= 0) {
                            std::cout << " ! Avion " << get_code() << " évite " << entry.first
                                << " (distance: " << distance_autre << "px)" << std::endl;
                            warnings_cooldown[entry.first] = 20;
                        }
                    }
                }
            }
        }

        for (auto& pair : warnings_cooldown) {
            if (pair.second > 0) pair.second--;
        }

        distance = std::sqrt(dx * dx + dy * dy);
        if (distance == 0) distance = 1;

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

        {
            std::lock_guard<std::mutex> lock(data->avions_positionsMutex);
            data->avions_positions[get_code()] = nouvellePos;
        }

        static int log_counter = 0;
        log_counter++;

        if (log_counter % 50 == 0) {
            std::cout << "\n[Avion " << get_code() << "]" << std::endl;
            std::cout << "  Position: " << nouvellePos << std::endl;
            std::cout << "  Destination: " << aeroport_destination << " " << destination_actuelle << std::endl;
            std::cout << "  Distance restante: " << distance << "px" << std::endl;
            std::cout << "  Carburant: " << get_carburant() << "L" << std::endl;
        }

        if (log_counter > 1000) log_counter = 0;
    }
}

void ThreadedAvion::decollage() {
    int timing = 100;
    std::cout << "Avion " << get_code() << " : Début de la séquence de décollage..." << std::endl;

    for (int i = 0; i < 11; i += 1) {
        set_vitesse(i * 100);
        set_altitude(i * 600);

        if (i < 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(timing));
        }
    }
    std::cout << "Avion " << get_code() << " : Décollage terminé. Altitude de croisière atteinte." << std::endl;
}

void ThreadedAvion::supprimerDuSysteme() {
    stop_thread_ = true;

    {
        std::lock_guard<std::mutex> lock(data->avions_positionsMutex);
        data->avions_positions.erase(get_code());
    }

    {
        std::lock_guard<std::mutex> lock(data->app_avionMutex);
        data->app_avionQueue.erase(get_code());
    }

    {
        std::lock_guard<std::mutex> lock(data->avion_appMutex);
        std::queue<AvionToAPP> temp_queue;
        while (!data->avion_appQueue.empty()) {
            AvionToAPP msg = data->avion_appQueue.front();
            data->avion_appQueue.pop();
            if (msg.avionCode != get_code()) {
                temp_queue.push(msg);
            }
        }
        data->avion_appQueue = std::move(temp_queue);
    }
}



ThreadedCCR::ThreadedCCR(std::shared_ptr<DataHub> sd) : shared_data_(sd) {}

void ThreadedCCR::run() {
    std::cout << "CCR démarré" << std::endl;

    while (!stop_thread_) {
        trafic_national();
        gerer_planning();
        conflits();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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

void ThreadedCCR::trafic_national() {
    std::lock_guard<std::mutex> lock(shared_data_->avions_positionsMutex);

    static int compteur = 0;
    if (compteur % 50 == 0 && !shared_data_->avions_positions.empty()) {
        std::cout << "\n=== CCR: " << shared_data_->avions_positions.size()
            << " avions en vol ===" << std::endl;
    }
    compteur++;
}

void ThreadedCCR::gerer_planning() {
    static int compteur_appel = 0;
    if (compteur_appel % 200 == 0) {
        planning();
    }
    compteur_appel += 1;
}

void ThreadedCCR::conflits() {}



ThreadedAPP::ThreadedAPP(const std::string& nom_aeroport, Coord pos_aero, std::shared_ptr<DataHub> sd) : APP(), shared_data_(sd), nom_aeroport_(nom_aeroport), position_aeroport_(pos_aero) {
        {
            std::lock_guard<std::mutex> lock(shared_data_->coordonneesAeroportsMutex);
            shared_data_->coordonneesAeroports[nom_aeroport_] = pos_aero;
        }
        std::cout << "APP " << nom_aeroport_ << " créée à position: " << pos_aero << std::endl;
}

void ThreadedAPP::run() {
    std::cout << "APP " << nom_aeroport_ << " démarrée" << std::endl;

    while (!stop_thread_) {
        message_de_Avion();
        message_de_TWR();
        trafic_aerien();
        collisions();

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

        const float SEUIL_APPROCHE = 200.0f;
        const float SEUIL_FINAL = 50.0f;
        const float SEUIL_ATTERRISSAGE = 10.0f;

        std::cout << "\nAPP " << nom_aeroport_ << " : Avion " << message.avionCode
            << " - Distance: " << distance << "px | Altitude: " << message.Altitude << "ft" << std::endl;

        if (distance < SEUIL_ATTERRISSAGE) {
            if (avions_en_atterrissage.find(message.avionCode) == avions_en_atterrissage.end()) {
                std::cout << "APP " << nom_aeroport_ << " : Demande atterrissage pour "
                    << message.avionCode << std::endl;
                demande_piste_TWR(message.avionCode);
                avions_en_atterrissage.insert(message.avionCode);
            }
        }
        else if (distance < SEUIL_FINAL) {
            std::cout << "APP " << nom_aeroport_ << " : Approche finale pour "
                << message.avionCode << std::endl;

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
        else if (distance < SEUIL_APPROCHE) {
            if (avions_en_attente.find(message.avionCode) == avions_en_attente.end()) {
                std::cout << "APP " << nom_aeroport_ << " : Mise en attente circulaire pour "
                    << message.avionCode << std::endl;
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
            std::cout << "APP " << nom_aeroport_ << " : Piste libre confirmée pour "
                << reponse.avionCode << std::endl;
            envoie_trajectoire_Avion(reponse.avionCode);
        }
        else {
            std::cout << "APP " << nom_aeroport_ << " : Piste occupée pour "
                << reponse.avionCode << ", maintien en attente" << std::endl;
            std::lock_guard<std::mutex> pos_lock(shared_data_->avions_positionsMutex);
            if (shared_data_->avions_positions.find(reponse.avionCode) != shared_data_->avions_positions.end()) {
                envoie_trajectoire_circulaire(reponse.avionCode,
                    shared_data_->avions_positions[reponse.avionCode]);
            }
        }
        lock.lock();
    }
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

    std::cout << "APP " << nom_aeroport_ << " envoie trajectoire atterrissage à " << code << std::endl;
}

void ThreadedAPP::demande_piste_TWR(const std::string& code) {
    APPToTWR_DemandePiste demande;
    demande.avionCode = code;

    {
        std::lock_guard<std::mutex> lock(shared_data_->app_twrMutex);
        shared_data_->app_twrQueue.push(demande);
    }
    shared_data_->app_twrCondition.notify_one();

    std::cout << "APP " << nom_aeroport_ << " demande piste pour " << code << std::endl;
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

    static int compteur = 0;
    if (compteur % 50 == 0 && !shared_data_->avions_positions.empty()) {
        std::cout << "APP " << nom_aeroport_ << " - Trafic: "
            << shared_data_->avions_positions.size() << " avions" << std::endl;
    }
    compteur++;
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
                std::cout << "\nAPP " << nom_aeroport_ << " : ALERTE COLLISION entre "
                    << avions_en_vol[i].first << " et " << avions_en_vol[j].first
                    << " (distance: " << distance << "px)" << std::endl;
            }
        }
    }
}



ThreadedTWR::ThreadedTWR(int nb_places, const std::string& nom_a, std::shared_ptr<DataHub> sd)
    : TWR(nb_places), shared_data_(sd), nom_aeroport_(nom_a) {
    initialiser_parkings();
}

void ThreadedTWR::initialiser_parkings() {
    std::lock_guard<std::mutex> lock(shared_data_->parkingsMutex);
    shared_data_->parkingsLibres[nom_aeroport_] = std::vector<bool>(NB_PARKINGS, true);
    shared_data_->avionsAuParking[nom_aeroport_] = std::vector<AvionAuParking>();
    std::cout << "TWR " << nom_aeroport_ << " : " << NB_PARKINGS << " parkings initialisés" << std::endl;
}

void ThreadedTWR::run() {
    std::cout << "TWR " << nom_aeroport_ << " démarrée" << std::endl;

    while (!stop_thread_) {
        message_de_APP();
        gerer_parking();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
        shared_data_->global_pisteLibreCondition.notify_all();
    }
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

            TWRToAPP_ReponsePiste reponse;
            reponse.avionCode = demande.avionCode;
            reponse.pisteLibre = true;

            {
                std::lock_guard<std::mutex> lock_twr(shared_data_->twr_appMutex);
                shared_data_->twr_appQueue.push(reponse);
            }
            shared_data_->twr_appCondition.notify_one();

            std::cout << "TWR " << nom_aeroport_ << " : Autorise atterrissage pour "
                << demande.avionCode << std::endl;

            std::this_thread::sleep_for(std::chrono::seconds(3));
            set_piste(true);
        }
        else {
            piste_lock.unlock();

            TWRToAPP_ReponsePiste reponse;
            reponse.avionCode = demande.avionCode;
            reponse.pisteLibre = false;

            {
                std::lock_guard<std::mutex> lock_twr(shared_data_->twr_appMutex);
                shared_data_->twr_appQueue.push(reponse);
            }
            shared_data_->twr_appCondition.notify_one();

            std::cout << "TWR " << nom_aeroport_ << " : Piste occupée, "
                << demande.avionCode << " en attente" << std::endl;
        }
        lock.lock();
    }
}

void ThreadedTWR::gerer_parking() {
    std::lock_guard<std::mutex> lock(shared_data_->parkingsMutex);

    auto& parkings = shared_data_->parkingsLibres[nom_aeroport_];
    auto& avionsAuParking = shared_data_->avionsAuParking[nom_aeroport_];

    int places_libres = std::count(parkings.begin(), parkings.end(), true);

    static int compteur = 0;
    if (compteur % 100 == 0) {
        std::cout << "\n[TWR " << nom_aeroport_ << "] Status:" << std::endl;
        std::cout << "  - Places libres: " << places_libres << "/" << parkings.size() << std::endl;
        std::cout << "  - Avions au parking: " << avionsAuParking.size() << std::endl;

        if (!avionsAuParking.empty()) {
            auto now = std::chrono::steady_clock::now();
            std::cout << "  - Détails parkings:" << std::endl;
            for (const auto& avion : avionsAuParking) {
                auto temps = std::chrono::duration_cast<std::chrono::seconds>(
                    now - avion.heureArrivee).count();
                std::cout << "    * " << avion.code << " au P" << (avion.numeroPlace + 1)
                    << " depuis " << temps << "s (départ prévu à "
                    << avion.tempsParkingSecondes << "s)" << std::endl;
            }
        }
    }
    compteur++;
}

void ThreadedTWR::gerer_decollages() {}



void SimulationManager::addAvion(const std::string& code_init, int alt_init, int vit_init,
    const Coord& pos_init, const Coord& dest_coord,
    int parking_init, int carb_init) {
    auto avion = std::make_unique<ThreadedAvion>(shared_data);
    avion->set_code(code_init);
    avion->set_altitude(alt_init);
    avion->set_vitesse(vit_init);
    avion->set_position(pos_init);
    avion->set_destination(dest_coord);
    avion->set_place_parking(parking_init);
    avion->set_carburant(carb_init);

    avions.push_back(std::move(avion));
}

void SimulationManager::addCCR() {
    auto ccr = std::make_unique<ThreadedCCR>(shared_data);
    ccrs.push_back(std::move(ccr));
}

void SimulationManager::addAPP(const std::string& airport_name, const Coord& position) {
    auto app = std::make_unique<ThreadedAPP>(airport_name, position, shared_data);
    apps.push_back(std::move(app));
    std::cout << "SimulationManager: APP " << airport_name << " ajoutée à " << position << std::endl;
}

void SimulationManager::addTWR(int nb_places, const std::string& airport_name) {
    auto twr = std::make_unique<ThreadedTWR>(nb_places, airport_name, shared_data);
    twrs.push_back(std::move(twr));
}

void SimulationManager::startSimulation() {
    std::cout << "Démarrage simulation ATC..." << std::endl;

    for (auto& ccr : ccrs) ccr->start();
    for (auto& app : apps) app->start();
    for (auto& twr : twrs) twr->start();
    for (auto& avion : avions) avion->start();
}

void SimulationManager::stopSimulation() {
    std::cout << "Arrêt simulation ATC..." << std::endl;

    for (auto& ccr : ccrs) ccr->stop();
    for (auto& app : apps) app->stop();
    for (auto& twr : twrs) twr->stop();
    for (auto& avion : avions) avion->stop();

    for (auto& ccr : ccrs) ccr->join();
    for (auto& app : apps) app->join();
    for (auto& twr : twrs) twr->join();
    for (auto& avion : avions) avion->join();
}