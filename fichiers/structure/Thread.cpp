#include "Thread.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <set>

Coord convertirCoordonneesRelatives(float relX, float relY,int largeurFenetre, int hauteurFenetre) {
    int x = static_cast<int>(relX * largeurFenetre);
    int y = static_cast<int>(relY * hauteurFenetre);
    return Coord(x, y);
}

void ThreadedAvion::run() {
    decollage();

    while (!stop_thread_) {
        if (stop_thread_) {
            break;
        }

        avancer();
        if (!stop_thread_) {
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
    std::cout << "Thread avion " << get_code() << " terminé." << std::endl;
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
            << get_carburant() << std::endl;

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
    if (stop_thread_) {
        return;
    }

    Coord position_actuelle = get_position();
    Coord destination_actuelle = get_destination();

    // Calcul de la distance à la destination
    int dx = destination_actuelle.get_x() - position_actuelle.get_x();
    int dy = destination_actuelle.get_y() - position_actuelle.get_y();

    float distance = std::sqrt(dx * dx + dy * dy);

    std::string aeroport_destination = "INCONNU";

    const int TOLERANCE = 10;
    
    if (std::abs(destination_actuelle.get_x() - static_cast<int>(0.54f * 1920)) <= TOLERANCE &&
        std::abs(destination_actuelle.get_y() - static_cast<int>(0.05f * 1080)) <= TOLERANCE) {
        aeroport_destination = "LILLE";
    }
    else if (std::abs(destination_actuelle.get_x() - static_cast<int>(0.5f * 1920)) <= TOLERANCE &&
        std::abs(destination_actuelle.get_y() - static_cast<int>(0.25f * 1080)) <= TOLERANCE) {
        aeroport_destination = "PARIS";
    }
    else if (std::abs(destination_actuelle.get_x() - static_cast<int>(0.84f * 1920)) <= TOLERANCE &&
        std::abs(destination_actuelle.get_y() - static_cast<int>(0.27f * 1080)) <= TOLERANCE) {
        aeroport_destination = "STRASBOURG";
    }
    else if (std::abs(destination_actuelle.get_x() - static_cast<int>(0.23f * 1920)) <= TOLERANCE &&
        std::abs(destination_actuelle.get_y() - static_cast<int>(0.34f * 1080)) <= TOLERANCE) {
        aeroport_destination = "RENNES";
    }
    else if (std::abs(destination_actuelle.get_x() - static_cast<int>(0.83f * 1920)) <= TOLERANCE &&
        std::abs(destination_actuelle.get_y() - static_cast<int>(0.83f * 1080)) <= TOLERANCE) {
        aeroport_destination = "NICE";
    }
    else if (std::abs(destination_actuelle.get_x() - static_cast<int>(0.40f * 1920)) <= TOLERANCE &&
        std::abs(destination_actuelle.get_y() - static_cast<int>(0.85f * 1080)) <= TOLERANCE) {
        aeroport_destination = "TOULOUSE";
    }

	//Atterrissage
    if (distance < 5) {
        if (!stop_thread_) {
            std::cout << "\n" << std::string(50, '=') << std::endl;
            std::cout << "   AVION " << get_code() << " ATTERRIT A " << aeroport_destination << std::endl;
            std::cout << "   Position finale: " << position_actuelle << std::endl;
            std::cout << "   Distance parcourue: " << "N/A" << std::endl;
            std::cout << std::string(50, '=') << std::endl;

            // 1. Supprimer de la surveillance des positions
            {
                std::lock_guard<std::mutex> lock(data->avions_positionsMutex);
                data->avions_positions.erase(get_code());
            }

            // 2. Supprimer des files d'attente de l'APP
            {
                std::lock_guard<std::mutex> lock(data->app_avionMutex);
                data->app_avionQueue.erase(get_code());
            }

            // 3. Supprimer des autres structures si nécessaire
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

            // 4. Arrêter définitivement le thread
            stop_thread_ = true;

            // Log supplémentaire
            std::cout << "Avion " << get_code()
                << " retiré de toutes les surveillances." << std::endl;

            return;
        }
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

                    // Si trop proche d'un autre avion
                    if (distance_autre < 25.0) {
                        vitesse = vitesse * 0.4f;

                        // Ajuster la direction pour s'éloigner
                        float force_repulsion = 15.0f / distance_autre;
                        dx -= static_cast<int>((entry.second.get_x() - position_actuelle.get_x()) * force_repulsion);
                        dy -= static_cast<int>((entry.second.get_y() - position_actuelle.get_y()) * force_repulsion);

                        if (warnings_cooldown[entry.first] <= 0) {
                            std::cout << " ! Avion " << get_code() << " évite " << entry.first
                                << " (distance: " << distance_autre << "px)" << std::endl;
                            warnings_cooldown[entry.first] = 20;  // Cooldown de 20 itérations
                        }
                    }
                }
            }
        }

        for (auto& pair : warnings_cooldown) {
            if (pair.second > 0) pair.second--;
        }

        distance = std::sqrt(dx * dx + dy * dy);
        if (distance == 0) distance = 1;  // Éviter la division par zéro

        //Calcul mouvement
        float vitesse_ajustee = vitesse;
        if (distance < 100) {
            vitesse_ajustee = vitesse * (distance / 100.0f);
        }

        float ratio = (vitesse_ajustee * 0.03f) / distance;
        int moveX = static_cast<int>(dx * ratio);
        int moveY = static_cast<int>(dy * ratio);

        // Assurer un mouvement minimum
        if (moveX == 0 && dx != 0) moveX = (dx > 0) ? 1 : -1;
        if (moveY == 0 && dy != 0) moveY = (dy > 0) ? 1 : -1;

        // Mouvement
        Coord nouvellePos(position_actuelle.get_x() + moveX,
            position_actuelle.get_y() + moveY);
        set_position(nouvellePos);

        // Mettre à jour la position partagée
        {
            std::lock_guard<std::mutex> lock(data->avions_positionsMutex);
            data->avions_positions[get_code()] = nouvellePos;
        }

        // ===== LOGS =====
        static int log_counter = 0;
        log_counter++;

        if (log_counter % 50 == 0) {
            std::cout << "\n[Avion " << get_code() << "]" << std::endl;
            std::cout << "  Position: " << nouvellePos << std::endl;
            std::cout << "  Destination: " << aeroport_destination << " " << destination_actuelle << std::endl;
            std::cout << "  Distance restante: " << distance << "px" << std::endl;
            std::cout << "  Vitesse: " << vitesse_ajustee << " px/cycle" << std::endl;
            std::cout << "  Carburant: " << get_carburant() << std::endl;
        }

        else if (log_counter % 10 == 0 && distance < 300) {
            std::cout << "Avion " << get_code()
                << " → " << aeroport_destination << " (distance: " << (int)distance << "px)" << std::endl;
        }

        if (log_counter > 1000) log_counter = 0;
    }
    else {
        if (rand() % 20 == 0) {
            std::cout << "Avion " << get_code()
                << " en approche finale à " << aeroport_destination
                << " (" << distance << "px)" << std::endl;
        }
    }
}

void ThreadedAvion::decollage() {
    int timing = 1000; //1s

    std::cout << "Avion " << get_code() << " : Début de la séquence de décollage..." << std::endl;

    for (int i = 0; i < 11; i += 1) {
        set_vitesse(i * 100);
        set_altitude(i * 600);

        std::cout << "Avion " << get_code() << " - Étape de décollage " << i << "/10. Alt: " << get_altitude() << "ft, Vit: " << get_vitesse() << std::endl;
        if (i < 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(timing));
        }
    }
    std::cout << "Avion " << get_code() << " : Décollage terminé. Altitude de croisière atteinte." << std::endl;
}

ThreadedAvion::ThreadedAvion(std::shared_ptr<DataHub> hub) : data(hub) {}

// CCR

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

    std::cout << "\n=== CCR SURVEILLANCE NATIONALE ===" << std::endl;

    for (const auto& entry : shared_data_->avions_positions) {
        std::cout << "CCR surveille: " << entry.first
            << " position: " << entry.second << std::endl;
    }

    std::cout << "=================================\n" << std::endl;
}

void ThreadedCCR::gerer_planning() {
    static int compteur_appel = 0;
    if (compteur_appel % 20 == 0) {
        planning();
        std::cout << "CCR - Planning aérien mis à jour" << std::endl;
    }
    compteur_appel += 1;
}

void ThreadedCCR::conflits() {}

// APP

ThreadedAPP::ThreadedAPP(const std::string& nom_aeroport, Coord pos_aero, std::shared_ptr<DataHub> sd)
    : APP(), shared_data_(sd), nom_aeroport_(nom_aeroport), position_aeroport_(pos_aero) {
    std::cout << "APP " << nom_aeroport_ << " créée à position: "
        << position_aeroport_ << std::endl;
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

        // Mettre à jour la position
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

        // Log clair
        std::cout << "\nAPP " << nom_aeroport_ << " : Avion " << message.avionCode
            << " - Distance: " << distance << "px | Altitude: " << message.Altitude
            << "ft" << std::endl;

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

            // Envoyer trajectoire directe
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
        else if (distance < SEUIL_APPROCHE) {// Mise en attente
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

    int nb_avions = shared_data_->avions_positions.size();
    if (nb_avions > 0) {
        std::cout << "APP " << nom_aeroport_ << " - Trafic: " << nb_avions << " avions en approche" << std::endl;
    }
}

void ThreadedAPP::collisions() {
    static std::set<std::string> avions_atterris;

    std::lock_guard<std::mutex> lock(shared_data_->avions_positionsMutex);

    // Filtrer les avions encore en vol
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
                std::cout << "\nAPP " << nom_aeroport_ << " : ALERTE COLLISION entre " << avions_en_vol[i].first << " et " << avions_en_vol[j].first << " (distance: " << distance << "px)" << std::endl;
            }
        }
    }
}

// TWR

ThreadedTWR::ThreadedTWR(int nb_places, const std::string& nom_a, std::shared_ptr<DataHub> sd) : TWR(nb_places), shared_data_(sd), nom_aeroport_(nom_a) {}

void ThreadedTWR::run() {
    std::cout << "TWR " << nom_aeroport_ << " démarrée avec "
        << trouver_place_libre() + 1 << " places de parking" << std::endl;

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
        std::cout << "TWR " << nom_aeroport_ << " - Piste libérée" << std::endl;
        shared_data_->global_pisteLibreCondition.notify_all();
    }
    else {
        std::cout << "TWR " << nom_aeroport_ << " - Piste occupée" << std::endl;
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
    static int compteur = 0;
    if (compteur % 10 == 0) {
        int place_libre = trouver_place_libre();
        if (place_libre != -1) {
            std::cout << "TWR " << nom_aeroport_ << " : Place de parking "
                << place_libre << " disponible" << std::endl;
        }
    }
    compteur++;
}

void ThreadedTWR::gerer_decollages() {
    static int timer = 0;
    if (timer % 30 == 0) {
        std::unique_lock<std::mutex> piste_lock(shared_data_->global_pisteLibre_mutex);
        if (shared_data_->global_pisteLibre) {
            shared_data_->global_pisteLibre = false;
            piste_lock.unlock();

            std::cout << "TWR " << nom_aeroport_ << " : Décollage en cours..." << std::endl;

            std::this_thread::sleep_for(std::chrono::seconds(2));

            set_piste(true);
            std::cout << "TWR " << nom_aeroport_ << " : Décollage terminé, piste libérée" << std::endl;
        }
    }
    timer++;
}

// SimulationManager

void SimulationManager::addAvion(const std::string& code_init, int alt_init, int vit_init,
    const Coord& pos_init, const std::string& dest_init,
    int parking_init, int carb_init) {
    Coord destination;
    if (dest_init == "Paris")
        destination = convertirCoordonneesRelatives(0.5f, 0.25f, 1920, 1080);
    else if (dest_init == "Nice")
        destination = convertirCoordonneesRelatives(0.83f, 0.83f, 1920, 1080);
    else if (dest_init == "Lille")
        destination = convertirCoordonneesRelatives(0.54f, 0.05f, 1920, 1080);
    else if (dest_init == "Strasbourg")
        destination = convertirCoordonneesRelatives(0.84f, 0.27f, 1920, 1080);
    else if (dest_init == "Rennes")
        destination = convertirCoordonneesRelatives(0.23f, 0.34f, 1920, 1080);
    else if (dest_init == "Toulouse")
        destination = convertirCoordonneesRelatives(0.40f, 0.85f, 1920, 1080);
    else
        destination = convertirCoordonneesRelatives(0.5f, 0.25f, 1920, 1080);

    auto avion = std::make_unique<ThreadedAvion>(shared_data);
    avion->set_code(code_init);
    avion->set_altitude(alt_init);
    avion->set_vitesse(vit_init);
    avion->set_position(pos_init);
    avion->set_destination(destination);
    avion->set_place_parking(parking_init);
    avion->set_carburant(carb_init);

    avions.push_back(std::move(avion));
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