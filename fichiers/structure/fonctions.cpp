#include "fonctions.hpp"
#include <algorithm>
#include <cmath>

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

Avion::Avion(const std::string& code_init, int alt_init, int vit_init, const Coord& pos_init,
	const Coord& dest_init, int parking_init, int carb_init)
	: code(code_init), altitude(alt_init), vitesse(vit_init), position(pos_init),
	destination(dest_init), place_parking(parking_init), carburant(carb_init) {
}

void CCR::planning() {
    using json = nlohmann::json;
    json planning;

    // Initialiser le générateur aléatoire
    srand(time(nullptr));

    std::vector<std::string> cities = { "Nice", "Rennes", "Paris", "Strasbourg", "Lille", "Toulouse" };

    planning["flights"] = json::array();

    int vol_number = 1000;

    // Générer un vol toutes les 15 minutes sur 24h
    for (int heure = 0; heure < 24; heure++) {
        for (int minute = 0; minute < 60; minute += 15) {
            // Choisir un aéroport de départ aléatoire
            int dep_idx = rand() % cities.size();
            int arr_idx;

            // Choisir une destination différente
            do {
                arr_idx = rand() % cities.size();
            } while (arr_idx == dep_idx);

            std::string departure = cities[dep_idx];
            std::string arrival = cities[arr_idx];

            // Calculer la durée du vol (approximative)
            std::string length;
            if (dep_idx == arr_idx) {
                length = "0h00";
            }
            else {
                // Durée entre 30 min et 2h selon la distance
                int duree_minutes = 30 + (rand() % 90);
                int h = duree_minutes / 60;
                int m = duree_minutes % 60;
                length = std::to_string(h) + "h" + (m < 10 ? "0" : "") + std::to_string(m);
            }

            std::string timeStr = std::to_string(heure) + "h" + (minute < 10 ? "0" : "") + std::to_string(minute);

            planning["flights"].push_back({
                {"day", "Lundi"},
                {"departure", departure},
                {"arrival", arrival},
                {"length", length},
                {"time", timeStr},
                {"code", "AF" + std::to_string(vol_number)}
                });

            vol_number++;
        }
    }

    std::ofstream file("planning.json");
    if (file.is_open()) {
        file << planning.dump(4);
        file.close();
        std::cout << "Planning genere avec " << planning["flights"].size() << " vols" << std::endl;
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

	int steps = std::max(std::abs(dx), std::abs(dy));

	if (steps > 0) {
		float stepX = static_cast<float>(dx) / steps;
		float stepY = static_cast<float>(dy) / steps;

		float current_x = static_cast<float>(x1);
		float current_y = static_cast<float>(y1);

		for (int i = 0; i <= steps; ++i) {
			path.push_back(Coord(static_cast<int>(current_x), static_cast<int>(current_y)));
			current_x += stepX;
			current_y += stepY;
		}
	}
}

bool TWR::get_piste() const {
	return piste;
}
void TWR::set_piste(bool facteur) {
	piste = facteur;
}

int TWR::trouver_place_libre() {
	for (size_t i = 0; i < parking.size(); i += 1) {
		if (parking[i]) return i;
	}
	return -1;
}

TWR::TWR(int nb_places) : piste(true), parking(nb_places, true) {}