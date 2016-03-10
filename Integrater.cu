#include "Integrater.h"
#include "cuda_call.h"
#include "Fix.h"
#include "WriteConfig.h"
// #include "globalDefs.h"

const string IntVerletType = "verlet";
const string IntRelaxType = "relax";


void Integrater::force(uint activeIdx) {
    
	int simTurn = state->turn;
	vector<Fix *> &fixes = state->fixes;
	for (Fix *f : fixes) {
		if (! (simTurn % f->applyEvery)) {
			f->compute();
		}
	}
};

void Integrater::forceSingle() {
	for (Fix *f : state->fixes) {
		if (f->forceSingle) {
			f->compute();
		}
	}
}


void Integrater::data() {
    /*
	int turn = state->turn;
	SHARED(DataManager) d = state->data;
	for (SHARED(DataSet) set : d->sets) {
		if (! ((turn - set->turnInit) % set->processEvery)) {
			set->process();
		}


	}
    */

}


void Integrater::singlePointEng() {
    GPUArray<float> &perParticleEng = state->gpd.perParticleEng;
    perParticleEng.d_data.memset(0);
	for (Fix *f : state->fixes) {
        f->singlePointEng(perParticleEng.getDevData());
    }

}
void Integrater::doDataCollection() {
    DataManager &dm = state->dataManager;
    if ((state->turn % dm.dataInterval) == 0) {
        if (dm.recordingEng()) {
            singlePointEng(); 
        }
    }
}
void Integrater::asyncOperations() {
    int turn = state->turn;
    auto dataAndWrite = [this] (int ts) { //well, if I try to use a local state pointer, this segfaults.  Need to capture this instead.  Little confused
        //have to set device in each thread
        state->devManager.setDevice(state->devManager.currentDevice, false);
        for (SHARED(WriteConfig) wc : state->writeConfigs) {
            if (not ((ts - wc->turnInit) % wc->writeEvery)) {
                wc->write(ts);
            }
        }
        for (SHARED(DataSet) ds : state->dataManager.userSets) {
            if (not ((ts - ds->turnInit) % ds->computeEvery)) {
                ds->process(ts);
            }
        }
    };
    bool needAsync = false;
    for (SHARED(WriteConfig) wc : state->writeConfigs) {
		if (not ((turn - wc->turnInit) % wc->writeEvery)) {
            needAsync = true;
            break;
		}
    }
    if (not needAsync) {
        for (SHARED(DataSet) ds : state->dataManager.userSets) {
            if (not ((turn - ds->turnInit) % ds->computeEvery)) {
                needAsync = true;
                break;
            }
        }
    }
    if (needAsync) {
        state->asyncHostOperation(dataAndWrite);
    }
}
/*
__global__ void printFloats(cudaTextureObject_t xs, int n) {
    int idx = GETIDX();
    if (idx < n) {
        int xIdx = XIDX(idx, sizeof(float4));
        int yIdx = YIDX(idx, sizeof(float4));
        float4 x = tex2D<float4>(xs, xIdx, yIdx);
        printf("idx %d, vals %f %f %f %d\n", idx, x.x, x.y, x.z, *(int *) &x.w);

    }
}
__global__ void printFloats(float4 *xs, int n) {
    int idx = GETIDX();
    if (idx < n) {
        float4 x = xs[idx];
        printf("idx %d, vals %f %f %f %f\n", idx, x.x, x.y, x.z, x.w);

    }
}
*/


void Integrater::basicPreRunChecks() {
    if (state->devManager.prop.major < 3 or (state->devManager.prop.major==3 and state->devManager.minor < 5)) {
        cout << "Device compute capability must be >= 3.5. Quitting" << endl;
        assert((state->devManager.prop.major == 3 and state->devManager.minor >= 5) or state->devManager.prop.majer > 3);
    }
    if (not state->grid.isSet) {
        cout << "Atom grid is not set!" << endl;
        assert(state->grid.isSet);
    }
    if (state->rCut == RCUT_INIT) {
        cout << "rcut is not set" << endl;
        assert(state->rCut != RCUT_INIT);
    }
    if (state->is2d and state->periodic[2]) {
        cout << "2d system cannot be periodic is z dimension" << endl;
        assert(not (state->is2d and state->periodic[2]));
    }
    for (int i=0; i<3; i++) {
        if (i<2 or (i==2 and state->periodic[2])) {
            if (state->grid.ds[i] < state->rCut + state->padding) {
                cout << "Grid dimension " << i << "has discretization smaller than rCut + padding" << endl;
                assert(state->grid.ds[i] >= state->rCut + state->padding);
            }
        }
    }
    state->grid.adjustForChangedBounds();
}

void Integrater::basicPrepare(int numTurns) {
    int nAtoms = state->atoms.size();
	state->runningFor = numTurns;
    state->runInit = state->turn; 
    //Add refresh atoms!
    state->updateIdxFromIdCache(); //for updating fix atom pointers, etc
    state->prepareForRun();
    for (Fix *f : state->fixes) {
        f->updateGroupTag();
        f->prepareForRun();
    }
    for (GPUArrayBase *dat : activeData) {
        dat->dataToDevice();
    }
    state->gridGPU.periodicBoundaryConditions(state->rCut + state->padding, true);
}

void Integrater::basicFinish() {
    if (state->asyncData && state->asyncData->joinable()) {
        state->asyncData->join();
    }
    for (GPUArrayBase *dat : activeData) {
        dat->dataToHost();
    }
    cudaDeviceSynchronize();
    state->downloadFromRun();
    for (Fix *f : state->fixes) {
        f->postRun();
    }

}
void Integrater::setActiveData() {
    activeData = vector<GPUArrayBase *>();
    activeData.push_back((GPUArrayBase *) &state->gpd.ids);
    activeData.push_back((GPUArrayBase *) &state->gpd.xs);
    activeData.push_back((GPUArrayBase *) &state->gpd.vs);
    activeData.push_back((GPUArrayBase *) &state->gpd.fs);
    activeData.push_back((GPUArrayBase *) &state->gpd.fsLast);
    activeData.push_back((GPUArrayBase *) &state->gpd.idToIdxs);
    activeData.push_back((GPUArrayBase *) &state->gpd.qs);
}

Integrater::Integrater(State *state_, string type_) : state(state_), type(type_){
    setActiveData(); 
}

void export_Integrater() {
    class_<Integrater, boost::noncopyable> ("Integrater")
        //.def("run", &Integrater::run)
        ;
}

