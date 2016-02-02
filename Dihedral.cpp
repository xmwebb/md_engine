#include "Dihedral.h"

DihedralOPLS::DihedralOPLS(Atom *a, Atom *b, Atom *c, Atom *d, double coefs_[4]) {
    atoms[0] = a;
    atoms[1] = b;
    atoms[2] = c;
    atoms[4] = d;
    for (int i=0; i<4; i++) {
        coefs[i] = coefs_[i];
    }

}

void DihedralOPLSGPU::takeIds(int *ids_) {
    ids[0] = ids_[0];
    ids[1] = ids_[1];
    ids[2] = ids_[2];
    ids[3] = ids_[3];
}
void DihedralOPLSGPU::takeValues(DihedralOPLS &other) {
    for (int i=0; i<4; i++) {
        coefs[i] = other.coefs[i];
    }
}