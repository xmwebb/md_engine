#include "InitializeAtoms.h"

default_random_engine InitializeAtoms::generator = default_random_engine();
/*
make a 'ready' flag in state, which means am ready to run.  creating atoms makes false, 
		make ready by re-doing all atom pointers 

		nah, am making ready on each
*/
int max_id(vector<Atom> &atoms) {
	int id = -1;
	for (Atom &a : atoms) {
		if (a.id > id) {
			id = a.id;
		}
	}
	return id;
}

void InitializeAtoms::populateOnGrid(SHARED(State) state, Bounds &bounds, string handle, int n) {
	assert(n>=0);
	vector<Atom> &atoms = state->atoms;


	int n_final = atoms.size() + n;

	int nPerSide = ceil(pow(n, 1.0/3.0));
	Vector deltaPerSide = bounds.trace / nPerSide;
	for (int i=0; i<nPerSide; i++) {
		for (int j=0; j<nPerSide; j++) {
			for (int k=0; k<nPerSide; k++) {
				if ((int) atoms.size() == n_final) {
					return;
				}
				Vector pos = bounds.lo + Vector(i, j, k) * deltaPerSide;
                state->addAtom(handle, pos, 0);
			}
		}
	}
	state->changedAtoms = true;
}
void InitializeAtoms::populateRand(SHARED(State) state, Bounds &bounds, string handle, int n, num distMin) {
	assert(n>=0);

	random_device randDev;
	generator.seed(randDev());
	vector<Atom> &atoms = state->atoms;
	AtomParams &params = state->atomParams;
	vector<string> handles = params.handles;
	int type = find(handles.begin(), handles.end(), handle) - handles.begin();

	assert(type != (int) handles.size()); //makes sure it found one
	unsigned int n_final = atoms.size() + n;
	uniform_real_distribution<num> dists[3];
	for (int i=0; i<3; i++) {
		dists[i] = uniform_real_distribution<num>(bounds.lo[i], bounds.hi[i]);
	}
	if (state->is2d) {
		dists[2] = uniform_real_distribution<num>(0, 0);
	}

	int id = max_id(atoms) + 1;
	while (atoms.size() < n_final) {
		Vector pos;
		for (int i=0; i<3; i++) {
			pos[i] = dists[i](generator);
		}
		bool is_overlap = false;
		for (Atom &a : atoms) {
			int typeA = a.type;
			if (a.pos.distSqr(pos) < distMin * distMin) {
				is_overlap = true;
				break;
			}
		}
		if (not is_overlap) {
            state->addAtom(handle, pos, 0);
			id++;
		}
	}
	if (state->is2d) {
		for (Atom &a: atoms) {
			a.pos[2]=0;
		}
	}
	state->changedAtoms = true;

}
void InitializeAtoms::initTemp(SHARED(State) state, string groupHandle, num temp) { //boltzmann const is 1 for reduced lj units
//	random_device randDev;
	generator.seed(2);//randDev());
    int groupTag = state->groupTagFromHandle(groupHandle);
	
	vector<Atom *> atoms = LISTMAPREFTEST(Atom, Atom *, a, state->atoms, &a, a.groupTag & groupTag);

    assert(atoms.size()>1);
	map<num, normal_distribution<num> > dists;
	for (Atom *a : atoms) {
		if (dists.find(a->mass) == dists.end()) {
			dists[a->mass] = normal_distribution<num> (0, sqrt(temp / a->mass));
		}
	}
	Vector sumVels;
	for (Atom *a : atoms) {
		for (int i=0; i<3; i++) {
			a->vel[i] = dists[a->mass](generator);
		}
		sumVels += a->vel;
	}
	sumVels /= (num) atoms.size();
	double sumKe = 0;
	for (Atom *a : atoms) {
		a->vel -= sumVels;
		sumKe += a->kinetic();
	}
	double curTemp = sumKe / 3.0 / atoms.size();
	for (Atom *a : atoms) {
		a->vel *= (num) sqrt(temp / curTemp);
	}
	if (state->is2d) {
		for (Atom *a : atoms) {
			a->vel[2] = 0;
		}
	}


}

void export_InitializeAtoms() {
    class_<InitializeAtomsPythonWrap> ("InitializeAtoms")
        //	.def("populateOnGrid", &InitializeAtoms::populateOnGrid, (python::arg("bounds"), python::arg("handle"), python::arg("n")) )
        //	.staticmethod("populateOnGrid")
        .def("populateRand", &InitializeAtoms::populateRand, (python::arg("bounds"), python::arg("handle"), python::arg("n"), python::arg("distMin")) )
        .staticmethod("populateRand")
        .def("initTemp", &InitializeAtoms::initTemp, (python::arg("groupHandle"), python::arg("temp")) )
        .staticmethod("initTemp")
        ;
}