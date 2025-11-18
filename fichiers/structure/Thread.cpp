#include "Thread.hpp"

void ThreadedAvion::run() {
	while (!stop_thread_) {
		//this->avancer_trajectoire
		AvionToAPP mess; //pour envoyer les données à l'APP voulue
		mess.avionCode = get_code();
		mess.Position = get_position();
		mess.Altitude = get_altitude();
		{
			std::lock_guard<std::mutex> lock(data->avion_appMutex);
			data->avion_appQueue.push(mess);
		}
		data->avion_appCondition.notify_one();

		//check_messages_from_app();
		//gerer_carburant_et_urgences();

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}