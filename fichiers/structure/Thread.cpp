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

	if (distance > 2) {
		float ratio = (get_vitesse() * 0.1f) / distance;
		int moveX = static_cast<int>(dx * ratio);
		int moveY = static_cast<int>(dy * ratio);

		if (moveX == 0 && dx != 0) moveX = (dx > 0) ? 1 : -1;
		if (moveY == 0 && dy != 0) moveY = (dy > 0) ? 1 : -1;

		Coord nouvellePos(get_position().get_x() + moveX, get_position().get_y() + moveY); set_position(nouvellePos);
		{
			std::lock_guard<std::mutex> lock(data->avions_positionsMutex);
			data->avions_positions[get_code()] = nouvellePos;
		}
	}
	else {
		std::cout << "Avion " << get_code() << " arrivé à destination!" << std::endl;
		stop_thread_ = true;
	}
}

ThreadedAvion::ThreadedAvion(std::shared_ptr<DataHub> hub) : data(hub) {}

//CCR 

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

void ThreadedCCR::trafic_national(){
	std::lock_guard<std::mutex> lock(shared_data_->avions_positionsMutex);

	std::cout << "=== CCR SURVEILLANCE NATIONALE ===" << std::endl;

	for (const auto& entry : shared_data_->avions_positions) {
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

ThreadedAPP::ThreadedAPP(const std::string& nom_aeroport, Coord pos_aero, std::shared_ptr<DataHub> sd) : APP(), shared_data_(sd), nom_aeroport_(nom_aeroport), position_aeroport_(pos_aero) {}

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
	std::unique_lock<std::mutex> lock(shared_data_->avion_appMutex);

	while (!shared_data_->avion_appQueue.empty()) {
		AvionToAPP message = shared_data_->avion_appQueue.front();
		shared_data_->avion_appQueue.pop();
		lock.unlock();
		{
			std::lock_guard<std::mutex> position_lock(shared_data_->avions_positionsMutex);
			shared_data_->avions_positions[message.avionCode] = message.Position;
		}

		double distance = std::sqrt(
			std::pow(message.Position.get_x() - position_aeroport_.get_x(), 2) +
			std::pow(message.Position.get_y() - position_aeroport_.get_y(), 2)
		);

		if (distance < 50.0) {
			std::cout << "APP " << nom_aeroport_ << " : Avion " << message.avionCode << " est proche (" << distance << "). ATTERRISSAGE AUTORISE !" << std::endl;
			{
				std::lock_guard<std::mutex> position_lock(shared_data_->avions_positionsMutex);
				shared_data_->avions_positions.erase(message.avionCode);
			}

			// passer "parking"
		}
		else {
		}
		lock.lock();
	}
}
void ThreadedAPP::message_de_TWR() {


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
	std::lock_guard<std::mutex> lock(shared_data_->avions_positionsMutex);
	std::vector<std::string> avions_codes;
	for (const auto& entry : shared_data_->avions_positions) {
		avions_codes.push_back(entry.first);
	}

	for (size_t i = 0; i < avions_codes.size(); i+=1) {
		for (size_t j = i + 1; j < avions_codes.size(); j+=1) {
			const Coord& pos1 = shared_data_->avions_positions[avions_codes[i]];
			const Coord& pos2 = shared_data_->avions_positions[avions_codes[j]];

			int dx = pos1.get_x() - pos2.get_x();
			int dy = pos1.get_y() - pos2.get_y();
			float distance = std::sqrt(dx * dx + dy * dy);

			if (distance < 10) {
				std::cout << "APP " << nom_aeroport_ << " : RISQUE COLLISION entre "
					<< avions_codes[i] << " et " << avions_codes[j]
					<< " (distance: " << distance << ")" << std::endl;
			}
		}
	}
}


//TWR 


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
void ThreadedTWR::message_de_APP(){}
void ThreadedTWR::gerer_parking(){}
void ThreadedTWR::gerer_decollages(){}


//simulationManager


void SimulationManager::addAvion(const std::string& code_init, int alt_init, int vit_init, const Coord& pos_init, const std::string& dest_init, int parking_init, int carb_init) {
	Coord destination;
	if (dest_init == "Paris") destination = Coord(960, 270);
	else if (dest_init == "Nice") destination = Coord(1594, 896);
	else if (dest_init == "Lille") destination = Coord(1037, 54);
	else if (dest_init == "Strasbourg") destination = Coord(1613, 292);
	else if (dest_init == "Rennes") destination = Coord(442, 367);
	else if (dest_init == "Toulouse") destination = Coord(768, 918);

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
	if (airport_name == "Paris") position_aero = Coord(250, 100);
	else if (airport_name == "Nice") position_aero = Coord(400, 350);
	else if (airport_name == "Lille") position_aero = Coord(1037, 54);
	else if (airport_name == "Strasbourg") position_aero = Coord(1613, 292);
	else if (airport_name == "Rennes") position_aero = Coord(442, 367);
	else if (airport_name == "Toulouse") position_aero = Coord(768, 918);

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