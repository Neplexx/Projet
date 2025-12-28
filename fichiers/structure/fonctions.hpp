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
	void set_x(int X); // d�finit la coordonn�e x
	void set_y(int Y); // d�finit la coordonn�e y
	int get_x() const; // retourne la coordonn�e x
	int get_y() const; // retourne la coordonn�e y
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
	void calcul_trajectoire(Avion& avion); // calcul d'une trajectoire sans collision pour l'avion
	void planning();
};

class APP {
public:
	void envoie_infos_avion(); // envoie la trajectoire et altitude � suivre � l'avion apr�s r�ception du signal
	void piste_libre(); // demande � twr si la piste d'atterrissage est libre
	void urgence();
	void trajectoire_atterissage();

};

class TWR {
private:
	bool piste; // vrai si libre, false sinon
	std::vector<bool> parking;
	void set_piste(bool facteur);
public:
	bool get_piste() const;

	int trouver_place_libre();

	void journal();

	TWR(int nb_places);
};
#endif