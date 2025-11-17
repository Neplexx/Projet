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

Coord::Coord(int X, int Y) : x(X) , y(X) {}

bool Coord::operator==(const Coord& other) const { 
	return x == other.x && y == other.y; 
}
std::ostream& operator<<(std::ostream& os, const Coord& c) {
	os << "(" << c.x << ", " << c.y << ")";
	return os;
};

void CCR::planning() {
	using json = nlohmann::json;
	json planning;
	planning["Days"] = { "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi", "Dimanche" };
	planning["Arrival"] = { "Nice", "Rennes", "Paris", "Strasbourg", "Lille", "Toulouse" };
	planning["Derparture"] = planning["Arrival"];
	planning["Lenght"] = { {"1h45", "1h45", "1h30", "1h30", "1h15"}, {"1h45","0h45","1h45","1h15","1h30"}, {"1h45","0h45","1h00","0h30","1h15"}, {"1h30","1h45","1h00","1h00","2h00"},{"1h30", "1h15", "0h30", "1h00", "1h30" },{"1h15", "1h30", "1h15", "2h00", "1h30"}};
	planning["AirplaneNumber"] = { "60", "12", "180", "10", "18", "60"};
	planning["Time"] = { "0h00", "0h15", "0h30", "0h45" };
}


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
std::string Avion::get_destination() const {
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
void Avion::set_destination(const std::string& dest) {
	destination = dest;
}
void Avion::set_place_parking(int place) {
	place_parking = place;
}
void Avion::set_carburant(int carb) {
	carburant = carb;
}

Avion::Avion(const std::string& code_init, int alt_init, int vit_init, const Coord& pos_init, const std::string& dest_init, int parking_init, int carb_init) : code(code_init), altitude(alt_init), vitesse(vit_init), position(pos_init), destination(dest_init), place_parking(parking_init), carburant(carb_init) {}


//Fonctions TWR

bool TWR::get_piste() const {
	return piste;
}
void TWR::set_piste(bool facteur) {
	piste = facteur;
}

int TWR::trouver_place_libre() {
	for (int i = 0; i < parking.size(); i+=1) {
		if (parking[i]) return i;
	}
	return -1;
}

TWR::TWR(int nb_places) : piste(true), parking(nb_places, true){}



