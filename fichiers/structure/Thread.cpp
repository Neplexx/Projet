#include "Thread.hpp"

void ThreadedAvion::run() {
	while (!stop_thread_) {
		//this->trajectoire_atterissage
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

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}
void ThreadedAvion::message_de_APP() {
	std::unique_lock<std::mutex> lock(data->app_avionMutex);

	auto it = data->app_avionQueue.find(get_code());
	if (it != data->app_avionQueue.end() && !it->second.empty()) {
		APPToAvion message = it->second.front();
		it->second.pop();
		lock.unlock();

		if (!message.trajectoire.empty()) {
			std::cout << "Avion " << get_code() << " reçoit nouvelle trajectoire de " << message.trajectoire.size() << " points" << std::endl;
			//mettre trajectoire maj
		}

		if (message.objectifAltitude > 0) {
			std::cout << "Avion " << get_code() << " objectif altitude: " << message.objectifAltitude << std::endl;
			//fonction pour modifier la traectoire également
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
		std::cout << "AVION " << get_code() << " CARBURANT FAIBLE: " << get_carburant() << std::endl;

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
	int dx = get_destination().get_x() - get_position().get_x();
	int dy = get_destination().get_y() - get_position().get_y();

	float distance = std::sqrt(dx * dx + dy * dy);

	if (distance > 5) {
		float ratio = (get_vitesse() * 0.05f) / distance;
		int moveX = static_cast<int>(dx * ratio);
		int moveY = static_cast<int>(dy * ratio);

		Coord nouvellePos(get_position().get_x() + moveX, get_position().get_y() + moveY);
		set_position(nouvellePos);
	}
}

ThreadedAvion::ThreadedAvion(std::shared_ptr<DataHub> hub) : data(hub) {}

//CCR 

ThreadedCCR::ThreadedCCR(DataHub& sd) : shared_data_(sd) {}


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

void ThreadedCCR::trafic_national(){
	std::lock_guard<std::mutex> lock(shared_data_.avions_positionsMutex);

	std::cout << "=== CCR SURVEILLANCE NATIONALE ===" << std::endl;

	for (const auto& entry : shared_data_.avions_positions) {
		std::cout << "CCR surveille: " << entry.first << " position: " << entry.second << " altitude: " << "N/A" << std::endl;
	}

	std::cout << "=================================" << std::endl;
}

void ThreadedCCR::gerer_planning(){
	static int compteur_appel = 0;
	if (compteur_appel % 20 == 0) {
		planning();
		std::cout << "CCR - Planning aérien mis à jour" << std::endl;
	}
	compteur_appel += 1;
}
void ThreadedCCR::conflits(){}

//APP 

ThreadedAPP::ThreadedAPP(const std::string& nom_aeroport, Coord pos_aero, DataHub& sd) : APP(), shared_data_(sd), nom_aeroport_(nom_aeroport), position_aeroport_(pos_aero) {}

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
	std::unique_lock<std::mutex> lock(shared_data_.avion_appMutex);

	while (!shared_data_.avion_appQueue.empty()) {
		AvionToAPP message = shared_data_.avion_appQueue.front();
		shared_data_.avion_appQueue.pop();
		lock.unlock();
		{
			std::lock_guard<std::mutex> position_lock(shared_data_.avions_positionsMutex);
			shared_data_.avions_positions[message.avionCode] = message.Position;
		}

		double distance = std::sqrt(
			std::pow(message.Position.get_x() - position_aeroport_.get_x(), 2) +
			std::pow(message.Position.get_y() - position_aeroport_.get_y(), 2)
		);

		if (distance < 50.0) {
			std::cout << "APP " << nom_aeroport_ << " : Avion " << message.avionCode << " est proche (" << distance << "). ATTERRISSAGE AUTORISE !" << std::endl;
			{
				std::lock_guard<std::mutex> position_lock(shared_data_.avions_positionsMutex);
				shared_data_.avions_positions.erase(message.avionCode);
			}

			// passer "parking"
		}
		else {
		}
		lock.lock();
	}
}
void ThreadedAPP::envoie_trajectoire_Avion(const std::string& code){}

void ThreadedAPP::demande_piste_TWR(const std::string& code) {
	APPToTWR_DemandePiste demande;
	demande.avionCode = code;

	{
		std::lock_guard<std::mutex> lock(shared_data_.app_twrMutex);
		shared_data_.app_twrQueue.push(demande);
	}
	shared_data_.app_twrCondition.notify_one();

	std::cout << "APP " << nom_aeroport_ << " demande piste pour " << code << std::endl;
}
void ThreadedAPP::message_de_TWR(){}

void ThreadedAPP::trafic_aerien() {
	std::lock_guard<std::mutex> lock(shared_data_.avions_positionsMutex);

	int nb_avions = shared_data_.avions_positions.size();
	if (nb_avions > 0) {
		std::cout << "APP " << nom_aeroport_ << " - Trafic: " << nb_avions << " avions en approche" << std::endl;
	}
}
void ThreadedAPP::collisions(){}


//TWR 


ThreadedTWR::ThreadedTWR(int nb_places, const std::string& nom_a, DataHub& sd)
	: TWR(nb_places), shared_data_(sd), nom_aeroport_(nom_a) {
}

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
	std::lock_guard<std::mutex> lock(shared_data_.global_pisteLibre_mutex);
	shared_data_.global_pisteLibre = facteur;

	if (facteur) {
		std::cout << "TWR " << nom_aeroport_ << " - Piste libérée" << std::endl;
		shared_data_.global_pisteLibreCondition.notify_all();
	}
	else {
		std::cout << "TWR " << nom_aeroport_ << " - Piste occupée" << std::endl;
	}
}
void ThreadedTWR::message_de_APP(){}
void ThreadedTWR::gerer_parking(){}
void ThreadedTWR::gerer_decollages(){}