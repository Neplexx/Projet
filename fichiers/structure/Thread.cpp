#include "Thread.hpp"

void ThreadedAvion::run() {
	while (!stop_thread_) {
		//this->trajectoire_atterissage
		AvionToAPP mess; //pour envoyer les données à l'APP voulue
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

void ThreadedCCR::trafic_national(){}
void ThreadedCCR::gerer_planning(){}
void ThreadedCCR::conflits(){}

//APP 

ThreadedAPP::ThreadedAPP(const std::string& nom_aeroport, DataHub& sd) : APP(), shared_data_(sd), nom_aeroport_(nom_aeroport) {}

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

void ThreadedAPP::message_de_Avion(){}
void ThreadedAPP::envoie_trajectoire_Avion(){}
void ThreadedAPP::demande_piste_TWR(const std::string& code){}
void ThreadedAPP::message_de_TWR(){}
void ThreadedAPP::trafic_aerien(){}
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

void ThreadedTWR::set_piste(bool facteur){}
void ThreadedTWR::message_de_APP(){}
void ThreadedTWR::gerer_parking(){}
void ThreadedTWR::gerer_decollages(){}