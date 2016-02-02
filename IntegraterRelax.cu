
#include "IntegraterRelax.h"
#include "cutils_func.h"


IntegraterRelax::IntegraterRelax(SHARED(State) state_) : Integrater(state_.get(), IntRelaxType) {
}


//


//kernel for FIRE relax
//VDotF by hand
__global__ void vdotF_cu (float *dest, float4 *vs,float4 *fs, int n) {
    extern __shared__ float tmp[]; //should have length of # threads in a block (PERBLOCK)
    int potentialIdx = blockDim.x*blockIdx.x + threadIdx.x;
    if (potentialIdx < n) {
        tmp[threadIdx.x] =dot ( make_float3(vs[blockDim.x*blockIdx.x + threadIdx.x]),make_float3(fs[blockDim.x*blockIdx.x + threadIdx.x]) ) ;
    } else {
        tmp[threadIdx.x] = 0;
    }
    __syncthreads();
    int maxLookahead = log2f(blockDim.x-1);
    for (int i=0; i<=maxLookahead; i++) {
        int curLookahead = powf(2, i);
        if (! (threadIdx.x % (curLookahead*2))) {
            tmp[threadIdx.x] += tmp[threadIdx.x + curLookahead];
        }
        __syncthreads();
    }
    if (threadIdx.x == 0) {
        atomicAdd(dest, tmp[0]);
    }
}


__global__ void FIRE_new_vel_cu(int nAtoms, float4 *vs, float4 *fs, float scale1, float scale2) {
    int idx = GETIDX();
    if (idx < nAtoms) {
        float4 vel = vs[idx];
        float4 force = fs[idx];
        float invmass = vel.w;
        float4 newVel = vel*scale1 + force*scale2;
        newVel.w = invmass;
        vs[idx] = newVel;
    }
}


__global__ void zero_vel_cu(int nAtoms, float4 *vs) {
    int idx = GETIDX();
    if (idx < nAtoms) {
        float4 vel = vs[idx];
        vs[idx] = make_float4(0.0f,0.0f,0.0f,vel.w);
    }
}

__global__ void FIRE_preForce_cu(int nAtoms, cudaSurfaceObject_t xs, float4 *vs, float4 *fs, float dt) {
    int idx = GETIDX();
    if (idx < nAtoms) {
        int xIdx = XIDX(idx, sizeof(float4));
        int yIdx = YIDX(idx, sizeof(float4));
        int xAddr = xIdx * sizeof(float4);
        float4 pos = surf2Dread<float4>(xs, xAddr, yIdx);

        float4 vel = vs[idx];
        float4 force = fs[idx];

        float invmass = vel.w;
        float groupTag = force.w;
        float id = pos.w;
        pos += vel * dt;
        pos.w = id;

        surf2Dwrite(pos, xs, xAddr, yIdx);
        float4 newVel = force * dt * invmass;
        newVel.w = invmass;
        vs[idx]=newVel;
        fs[idx] = make_float4(0, 0, 0, groupTag);
    }
}




double IntegraterRelax::run(int numTurns, num fTol) {
    //TODO include citation
    cout << "FIRE relaxation\n";
    basicPreRunChecks();  

    basicPrepare(numTurns);

    CUT_CHECK_ERROR("Fire init execution failed");//Debug feature, checks error code


    //FIRE parameters
    //TODO create interface for them
    int lastNegative = 0;
    double dt = state->dt;
    const double alphaInit = 0.1;
    double alpha = alphaInit;
    const double alphaShrink = 0.99;
    const double dtGrow = 1.1;
    const double dtShrink = 0.5;
    const int delay = 5;
    const double dtMax = 10 * dt;


    //assuming constant number of atoms during run
    int atomssize=state->atoms.size();
    int periodicInterval = state->periodicInterval;
    int nblock = NBLOCK(atomssize);
    int remainder = state->turn % periodicInterval;
    int turnInit = state->turn; 

    //set velocity to 0
    // 	state->gpd.vs.memsetByVal(make_float3(0.0f,0.0f,0.0f);
    zero_vel_cu <<<nblock, PERBLOCK>>>(atomssize,state->gpd.vs.getDevData());
    //this will actually print file and line
    CUT_CHECK_ERROR("zero_vel_cu kernel execution failed");

    //vars to store kernels outputs
    GPUArray<float>VDotV(1);
    GPUArray<float>VDotF(1);
    GPUArray<float>FDotF(1);
    GPUArray<float>force(1);


    //neiblist build?
    state->gridGPU.periodicBoundaryConditions(state->rCut + state->padding, true);

    for (int i=0; i<numTurns; i++) {
        //init to 0 on cpu and gpu

        VDotV.memsetByVal(0.0);
        VDotF.memsetByVal(0.0);
        FDotF.memsetByVal(0.0);
        //vdotF calc
        if (! ((remainder + i) % periodicInterval)) {
            state->gridGPU.periodicBoundaryConditions(state->rCut + state->padding, true);
        }
        asyncOperations();

        vdotF_cu <<<nblock, 
                 PERBLOCK,
                 sizeof(float)*PERBLOCK>>>(
                         VDotF.getDevData(),
                         state->gpd.vs.getDevData(),
                         state->gpd.fs.getDevData(),
                         atomssize);
        CUT_CHECK_ERROR("vdotF_cu kernel execution failed");
        VDotF.dataToHost();

        if (VDotF.h_data[0] > 0) {

            //VdotV calc
            sumVectorSqr3D<float,float4> <<<nblock,
                PERBLOCK,
                sizeof(float)*PERBLOCK>>>(
                        VDotV.getDevData(),
                        state->gpd.vs.getDevData(),
                        atomssize);
            CUT_CHECK_ERROR("vdotV_cu kernel execution failed");
            VDotV.dataToHost();

            //FdotF
            sumVectorSqr3D<float,float4> <<<nblock,
                PERBLOCK,
                sizeof(float)*PERBLOCK>>>(
                        FDotF.getDevData(),
                        state->gpd.fs.getDevData(),
                        atomssize);
            CUT_CHECK_ERROR("fdotF_cu kernel execution failed");
            FDotF.dataToHost();

            float scale1 = 1 - alpha;
            float scale2 = 0;
            if (FDotF.h_data[0] != 0) {
                scale2 = alpha * sqrt(VDotV.h_data[0] / FDotF.h_data[0]);
            }
            //set velocity to
            //a.vel = a.vel * scale1 + a.force * scale2;
            FIRE_new_vel_cu <<<nblock, PERBLOCK>>>(atomssize,
                    state->gpd.vs.getDevData(),
                    state->gpd.fs.getDevData(),
                    scale1,scale2);
            if (i - lastNegative > delay) {
                dt = fmin(dt*dtGrow, dtMax);
                alpha *= alphaShrink;

            }
        } else {
            lastNegative = i;
            dt *= dtShrink;
            alpha = alphaInit;
            //set velocity to 0
            // 			state->gpd.vs.memsetByVal(make_float3(0.0f,0.0f,0.0f);
            zero_vel_cu <<<nblock, PERBLOCK>>>(atomssize,state->gpd.vs.getDevData());
            CUT_CHECK_ERROR("zero_vel_cu kernel execution failed");

        }

        FIRE_preForce_cu <<<nblock, PERBLOCK>>>
            (atomssize,
             state->gpd.xs.getSurf(),
             state->gpd.vs.getDevData(),
             state->gpd.fs.getDevData(),
             // 				 state->gpd.fsLast.getDevData(),
             dt);
        CUT_CHECK_ERROR("FIRE_preForce_cu kernel execution failed");

        int activeIdx = state->gpd.activeIdx;
        Integrater::force(activeIdx);

        if (fTol > 0 and i > delay and not (i%delay)) { //only check every so often
            force.memsetByVal(0.0);

            sumVector3D<float,float4> <<<nblock,
                PERBLOCK,
                sizeof(float)*PERBLOCK>>>(
                        force.getDevData(),
                        state->gpd.fs.getDevData(),
                        atomssize);
            CUT_CHECK_ERROR("kernel execution failed");//Debug feature, check error code

            force.dataToHost();
            // 			cout<<"Fire relax: force="<<force<<"; turns="<<i<<'\n';

            if (force.h_data[0] < fTol) {
                basicFinish();

                return force.h_data[0];
            }
        } 

        //shout status
        if (state->verbose and not ((state->turn - turnInit) % state->shoutEvery)) {
            cout << "Turn " << (int) state->turn << " " << (int) (100 * (state->turn - turnInit) / (num) numTurns) << " percent done" << endl;
        }
        state->turn++;

    }

    force.memsetByVal(0.0);

    sumVector3D<float,float4> <<<nblock,
        PERBLOCK,
        sizeof(float)*PERBLOCK>>>(
                force.getDevData(),
                state->gpd.fs.getDevData(),
                atomssize);
    CUT_CHECK_ERROR("kernel execution failed");//Debug feature, check error code


    basicFinish();

    cout<<"Fire relax done: force="<<force.h_data[0]<<"; turns="<<numTurns<<'\n';
    return force.h_data[0];
}

void export_IntegraterRelax() {
    class_<IntegraterRelax, SHARED(IntegraterRelax), bases<Integrater> > ("IntegraterRelax", init<SHARED(State)>())
        .def("run", &IntegraterRelax::run)
        ;
}
