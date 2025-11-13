#include "fonctions.hpp"

void Coord::set_x(int X) {
	x = X;
}
void Coord::set_y(int Y) {
	y = Y;
}

const int Coord::get_x() {
	return x;
}
const int Coord::get_y() {
	return y;
}

Coord::Coord(int X, int Y) : x(X) , y(X) {}


//fonctions class Avion


Coord avion::get_position() const {
	return position;
}
std::string avion::get_code() const {
	return code;
}
int avion::get_altitude() const {
	return altitude;
}
int avion::get_vitesse() const {
	return vitesse;
}
std::string avion::get_destination() const {
	return destination;
}
int avion::get_place_parking() const {
	return place_parking;
}
int avion::get_carburant() const {
	return carburant;
}

void avion::set_position(const Coord& pos) {
	position = pos;
}
void avion::set_code(const std::string& c) {
	code = c;
}
void avion::set_altitude(int alt) {
	altitude = alt;
}
void avion::set_vitesse(int vit) {
	vitesse = vit;
}
void avion::set_destination(const std::string& dest) {
	destination = dest;
}
void avion::set_place_parking(int place) {
	place_parking = place;
}
void avion::set_carburant(int carb) {
	carburant = carb;
}

avion::avion(const std::string& code_init, int alt_init, int vit_init, const Coord& pos_init, const std::string& dest_init, int parking_init, int carb_init) : code(code_init), altitude(alt_init), vitesse(vit_init), position(pos_init), destination(dest_init), place_parking(parking_init), carburant(carb_init) {}


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

