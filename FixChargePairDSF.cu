#include "FixChargePairDSF.h"
// #include <cmath>

//Pairwise force shifted damped Coulomb
//Christopher J. Fennel and J. Daniel Gezelter J. Chem. Phys (124), 234104 2006
// Eqn 19.
//force calculation:
//  F=q_i*q_j*[erf(alpha*r    )/r^2   +2*alpha/sqrt(Pi)*exp(-alpha^2*r^2    )/r
//	      -erf(alpha*r_cut)/rcut^2+2*alpha/sqrt(Pi)*exp(-alpha^2*r_cut^2)/r_cut]

//or F=q_i*q_j*[erf(alpha*r    )/r^2   +A*exp(-alpha^2*r^2    )/r- shift*r]
//with   A   = 2*alpha/sqrt(Pi)
//     shift = erf(alpha*r_cut)/r_cut^3+2*alpha/sqrt(Pi)*exp(-alpha^2*r_cut^2)/r_cut^2


__global__ void compute_charge_pair_DSF_cu(int nAtoms, cudaTextureObject_t xs, float4 *fs, int *neighborIdxs, cudaTextureObject_t neighborlist, cudaTextureObject_t qs, float alpha, float rCut,float A, float shift, BoundsGPU bounds) {

    int idx = GETIDX();
    if (idx < nAtoms) {
        float4 posWhole = tex2D<float4>(xs, XIDX(idx, sizeof(float4)), YIDX(idx, sizeof(float4)));
        float3 pos = make_float3(posWhole);

        float3 forceSum = make_float3(0, 0, 0);
        float qi = tex2D<float>(qs, XIDX(idx, sizeof(float)), YIDX(idx, sizeof(float)));

        int start = neighborIdxs[idx];
        int end = neighborIdxs[idx+1];
        //printf("start, end %d %d\n", start, end);
        for (int i=start; i<end; i++) {
            int otherIdx = tex2D<int>(neighborlist, XIDX(i, sizeof(int)), YIDX(i, sizeof(int)));
            float3 otherPos = make_float3(tex2D<float4>(xs, XIDX(otherIdx, sizeof(float4)), YIDX(otherIdx, sizeof(float4))));
            //then wrap and compute forces!
            float3 dr = bounds.minImage(pos - otherPos);
            float lenSqr = lengthSqr(dr);
            //   printf("dist is %f %f %f\n", dr.x, dr.y, dr.z);
            if (lenSqr < rCut*rCut) {
                float len=sqrtf(lenSqr);
                float qj = tex2D<float>(qs, XIDX(otherIdx, sizeof(float)), YIDX(otherIdx, sizeof(float)));

                float r2inv = 1.0f/lenSqr;
                float rinv = 1.0f/len;
                float forceScalar = qi*qj*(erfcf((alpha*len))*r2inv+A*exp(-alpha*alpha*lenSqr)*rinv-shift)*rinv;

		
                float3 forceVec = dr * forceScalar;
                forceSum += forceVec;
            }

        }   
        fs[idx] += forceSum; //operator for float4 + float3

    }

}
FixChargePairDSF::FixChargePairDSF(SHARED(State) state_, string handle_, string groupHandle_) : FixCharge(state_, handle_, groupHandle_, chargePairDSF) {
   setParameters(0.25,9.0);
};

void FixChargePairDSF::setParameters(float alpha_,float r_cut_)
{
  alpha=alpha_;
  r_cut=r_cut_;
  A=1.1283791670955125739*alpha;// 2/sqrt(pi)*alpha
  shift=std::erfc(alpha*r_cut)/(r_cut*r_cut)+A*exp(-alpha*alpha*r_cut*r_cut)/(r_cut);
}

void FixChargePairDSF::compute() {
    int nAtoms = state->atoms.size();
    GPUData &gpd = state->gpd;
    GridGPU &grid = state->gridGPU;
    int activeIdx = gpd.activeIdx;
    int *neighborIdxs = grid.perAtomArray.ptr;
    compute_charge_pair_DSF_cu<<<NBLOCK(nAtoms), PERBLOCK>>>(nAtoms, gpd.xs.getTex(), gpd.fs(activeIdx), neighborIdxs, grid.neighborlist.tex, gpd.qs.getTex(), alpha,r_cut, A,shift, state->boundsGPU);


}


void export_FixChargePairDSF() {
    class_<FixChargePairDSF, SHARED(FixChargePairDSF), bases<FixCharge> > ("FixChargePairDSF", init<SHARED(State), string, string> (args("state", "handle", "groupHandle")))
        .def("setParameters", &FixChargePairDSF::setParameters, (python::arg("alpha"), python::arg("r_cut")))
        ;
}