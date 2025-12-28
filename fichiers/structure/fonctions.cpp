#include "fonctions.hpp"

void Coord::set_x(int X) {
	x = X;
}
void Coord::set_y(int Y) {
	y = Y;
}

int Coord::get_x() const {
	return x;
}
int Coord::get_y() const {
	return y;
}

Coord::Coord(int X, int Y) : x(X), y(Y) {}

bool Coord::operator==(const Coord& other) const {
	return x == other.x && y == other.y;
}
std::ostream& operator<<(std::ostream& os, const Coord& c) {
	os << "(" << c.x << ", " << c.y << ")";
	return os;
};


//fonctions class Avion

Coord Avion::get_position() const {
	return position;
}
std::string Avion::get_code() const {
	return code;
}
int Avion::get_altitude() const {
	return altitude;
}
int Avion::get_vitesse() const {
	return vitesse;
}
Coord Avion::get_destination() const {
	return destination;
}
int Avion::get_place_parking() const {
	return place_parking;
}
int Avion::get_carburant() const {
	return carburant;
}

void Avion::set_position(const Coord& pos) {
	position = pos;
}
void Avion::set_code(const std::string& c) {
	code = c;
}
void Avion::set_altitude(int alt) {
	altitude = alt;
}
void Avion::set_vitesse(int vit) {
	vitesse = vit;
}
void Avion::set_destination(const Coord& dest) {
	destination = dest;
}
void Avion::set_place_parking(int place) {
	place_parking = place;
}
void Avion::set_carburant(int carb) {
	carburant = carb;
}

Avion::Avion(const std::string& code_init, int alt_init, int vit_init, const Coord& pos_init, const Coord& dest_init, int parking_init, int carb_init) : code(code_init), altitude(alt_init), vitesse(vit_init), position(pos_init), destination(dest_init), place_parking(parking_init), carburant(carb_init) {}

// Fonctions CCR

void CCR::planning() {
	using json = nlohmann::json;
	json planning;

	srand(static_cast<unsigned int>(time(nullptr)));

	std::vector<std::string> cities = { "Nice", "Rennes", "Paris", "Strasbourg", "Lille", "Toulouse" };
	planning["flights"] = json::array();

	int vol_number = 1000;

	for (int heure = 0; heure < 24; heure++) {
		for (int minute = 0; minute < 60; minute += 15) {
			int dep_idx = rand() % cities.size();
			int arr_idx;
			do {
				arr_idx = rand() % cities.size();
			} while (arr_idx == dep_idx);

			std::string departure = cities[dep_idx];
			std::string arrival = cities[arr_idx];

			int duree_minutes = 30 + (rand() % 90);
			int h_dur = duree_minutes / 60;
			int m_dur = duree_minutes % 60;
			std::string length = std::to_string(h_dur) + "h" + (m_dur < 10 ? "0" : "") + std::to_string(m_dur);
			std::string timeStr = std::to_string(heure) + "h" + (minute < 10 ? "0" : "") + std::to_string(minute);

			planning["flights"].push_back({
				{"day", "Lundi"},
				{"departure", departure},
				{"arrival", arrival},
				{"length", length},
				{"time", timeStr},
				{"code", "AF" + std::to_string(vol_number)}
				});

			vol_number+=1;
		}
	}

	std::ofstream file("planning.json");
	if (file.is_open()) {
		file << planning.dump(4);
		file.close();
		std::cout << "Planning genere avec " << planning["flights"].size() << " vols" << std::endl;
	}
}

void journal(const std::string& type, const std::string& id, const std::string& action, const std::string& details = "") {
	using json = nlohmann::json;
	json log_entry = {
		{"timestamp", std::to_string(std::time(nullptr))},
		{"type", type},
		{"id", id},
		{"action", action},
		{"details", details}
	};
	std::ofstream journal_f("journal.json", std::ios::app);
	if (journal_f.is_open()) {
		journal_f << log_entry.dump() << ",\n";
		journal_f.close();
	}
	else {
		std::cerr << "Erreur : impossible d'ouvrir le fichier journal.json\n";
	}
}

void CCR::calcul_trajectoire(Avion& avion) {
	int nb_lignes = 300;
	int nb_colonnes = 500;

	std::vector<std::vector<int>> grille(nb_lignes, std::vector<int>(nb_colonnes, 0));
	grille[avion.get_position().get_y()][avion.get_position().get_x()] = 2;
	grille[avion.get_destination().get_y()][avion.get_destination().get_x()] = 3;

	int x1 = avion.get_position().get_x();
	int y1 = avion.get_position().get_y();
	int x2 = avion.get_destination().get_x();
	int y2 = avion.get_destination().get_y();
	std::vector<Coord> path;
	int dx = (x2 - x1);
	int dy = (y2 - y1);
	while (x1 != x2 || y1 != y2) {
		path.push_back(Coord(x1, y1));
		if (x1 != x2) x1 += dx;
		if (y1 != y2) y1 += dy;
	}
	int steps = std::max(std::abs(dx), std::abs(dy));

	float stepX = static_cast<float>(dx) / steps;
	float stepY = static_cast<float>(dy) / steps;

	float current_x = x1;
	float current_y = y1;

	for (int i = 0; i <= steps; i+=1) {
		path.push_back(Coord(static_cast<int>(current_x), static_cast<int>(current_y)));
		current_x += stepX;
		current_y += stepY;
	}

	std::cout << std::endl;
}

//Fonctions TWR

bool TWR::get_piste() const {
	return piste;
}
void TWR::set_piste(bool facteur) {
	piste = facteur;
}

int TWR::trouver_place_libre() {
	for (int i = 0; i < parking.size(); i += 1) {
		if (parking[i]) return i;
	}
	return -1;
}

TWR::TWR(int nb_places) : piste(true), parking(nb_places, true) {}


