#ifndef INTEGRATER_H
#define INTEGRATER_H
#include "Atom.h"
#include <vector>
//#include "Fix.h"
#include <chrono>
#include "GPUArray.h"
#include "GPUArrayTex.h"
#include "boost_for_export.h"
void export_Integrater();
class State;
using namespace std;


extern const string IntVerletType;
extern const string IntRelaxType;

class Integrater {

    protected:
        //virtual void preForce(uint);
        void force(uint);
        //virtual void postForce(uint);
        virtual void data();
        void asyncOperations();
        vector<GPUArrayBase *> activeData;
        void basicPreRunChecks();
        void basicPrepare(int);
        void basicFinish();
        void setActiveData();
	public:
		string type;
		State *state;
		Integrater() {};
		Integrater(State *state_, string type_);
       
		//double relax(int numTurns, num fTol);
		void forceSingle();
/*	void verletPreForce(vector<Atom *> &atoms, double timestep);
	void verletPostForce(vector<Atom *> &atoms, double timestep);
	void compute(vector<Fix *> &, int);
	void firstTurn(Run &params);
	void run(Run &params, int currentTurn, int numTurns);
	bool rebuildIsDangerous(vector<Atom *> &atoms, double);
	void addKineticEnergy(vector<Atom *> &, Data &);
	void setThermoValues(Run &);*/
};


#include "State.h"
#endif