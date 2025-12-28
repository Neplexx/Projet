#ifndef FONCTIONS_HPP
#define FONCTIONS_HPP

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "json.hpp"


class Coord {
private:
	int x;
	int y;
public:
	void set_x(int X);
	void set_y(int Y);
	int get_x() const;
	int get_y() const;
	Coord(int X = 0, int Y = 0);
	bool operator==(const Coord& other) const;
	friend std::ostream& operator<<(std::ostream& os, const Coord& c);
};

class Avion {
private:
	std::string code;
	int altitude;
	int vitesse;
	Coord position;
	Coord destination;
	int place_parking;
	int carburant;
public:
	Avion() = default;
	void set_position(const Coord& pos);
	Coord get_position() const;

	void set_code(const std::string& c);
	std::string get_code() const;

	void set_altitude(int alt);
	int get_altitude() const;

	void set_vitesse(int vit);
	int get_vitesse() const;

	void set_destination(const Coord& dest);
	Coord get_destination() const;

	void set_place_parking(int place);
	int get_place_parking() const;

	void set_carburant(int carb);
	int get_carburant() const;

	Avion(const std::string& code_init, int alt_init, int vit_init, const Coord& pos_init, const Coord& dest_init, int parking_init, int carb_init);
};

class CCR {
public:
	void calcul_trajectoire(Avion& avion);
	void planning();
};

class APP {};

class TWR {
private:
	bool piste; // vrai si libre, false sinon
	std::vector<bool> parking;
	void set_piste(bool facteur);
public:
	bool get_piste() const;
	int trouver_place_libre();
	TWR(int nb_places);
};
#endif