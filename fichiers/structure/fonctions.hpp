#include <iostream>
#include <vector>
#include <string>
#include "json.hpp"


class Coord {
private:
	int x;
	int y;
public:
	void set_x(int X); // définit la coordonnée x
	void set_y(int Y); // définit la coordonnée y
	int get_x() const; // retourne la coordonnée x
	int get_y() const; // retourne la coordonnée y
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
	std::string destination;
	int place_parking;
	int carburant;
public:
	void set_position(const Coord& pos);
	Coord get_position() const;

	void set_code(const std::string& c);
	std::string get_code() const;

	void set_altitude(int alt);
	int get_altitude() const;

	void set_vitesse(int vit);
	int get_vitesse() const;

	void set_destination(const std::string& dest);
	std::string get_destination() const;

	void set_place_parking(int place);
	int get_place_parking() const;

	void set_carburant(int carb);
	int get_carburant() const;
	void consommation_carburant(Coord position, Coord ancienne_position) const;

	Avion::Avion(const std::string& code_init, int alt_init, int vit_init, const Coord& pos_init, const std::string& dest_init, int parking_init, int carb_init);
};

class CCR { //International
public:
	void calcul_trajectoire(Avion& avion); // calcul d'une trajectoire sans collision pour l'avion
	void planning(); // gère le planning aérien dans un fichier .json
};

class APP { //Arrivée aéroport
public:
	void envoie_infos_avion(); // envoie la trajectoire et altitude à suivre à l'avion après réception du signal
	void piste_libre(); // demande à twr si la piste d'atterrissage est libre
	void urgence();
};

class TWR { //Dans aéroport
private:
	bool piste; // vrai si libre, false sinon
	std::vector<bool> parking; // index = la place et l'élément indique si la place est libre ou non
	void set_piste(bool facteur); // Protection de décollage pour que pas n'importe qui puisse libérer la piste
public:
	bool get_piste() const;

	int trouver_place_libre();

	void journal();

	void décollage();

	TWR(int nb_places);
};

//Ajouter class Thread

