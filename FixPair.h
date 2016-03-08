#ifndef FIX_PAIR_H
#define FIX_PAIR_H

#define DEFAULT_FILL INT_MAX

#include <climits>
#include <map>

#include "AtomParams.h"
#include "GPUArray.h"
#include "Fix.h"
#include "xml_func.h"

class State;

using namespace std;

/*! \brief Global function returning a single SquareVector item
 *
 * \param vals Pointer to SquareVector array
 * \param nCol Number of columns
 * \param i Row
 * \param j Column
 *
 * \returns Element (i,j) from vals
 *
 * This function returns a single element from a given SquareVector array.
 */
template <class T>
__host__ __device__ T squareVectorItem(T *vals, int nCol, int i, int j) {
    return vals[i*nCol + j];
}

/*! \brief Global function returning a reference to a single Square Vector item
 *
 * \param vals Pointer to SquareVector array
 * \param nCol Number of columns
 * \param i Row
 * \param j Column
 *
 * \returns Reference to element (i,j) from vals
 *
 * This function returns a reference to specific entry of a given SquareVector
 * array.
 */
template <class T>
__host__ __device__ T &squareVectorRef(T *vals, int nCol, int i, int j) {
    return vals[i*nCol + j];
}

/*! \namespace SquareVector
 * \brief Two dimensional array
 *
 * This namespace contains functions to create std::vector elements which can
 * be treated as two-dimensional arrays.
 */
namespace SquareVector {

    /*! \brief Create SquareVector
     *
     * \param size Number of rows and columns
     *
     * \returns New SquareVector
     *
     * This function creates a new SquareVector
     */
    template <class T>
    vector<T> create(int size) {
        return vector<T>(size*size, DEFAULT_FILL);
    }

    /*! \brief Fill SquareVector with values
     *
     * \param vec Pointer to SquareVector
     * \param size Number of Rows/Columns of the SquareVector
     * \param fillFunction Funtion pointer to the function used to determine
     *                     the elements in the SquareVector
     *
     * This function can be used to set all values of the SquareVector at once
     * using a function pointer.
     */
    template <class T>
    void populate(vector<T> *vec, int size,
                  std::function<T (T, T)> fillFunction) {
        for (int i=0; i<size; i++) {
            for (int j=0; j<size; j++) {
                T val = squareVectorRef<T>(vec->data(), size, i, j);
                if (i==j) {
                    if (val == DEFAULT_FILL) {
                        cout << "You have not defined interaction parameters "
                                "for atom type with index " << i << endl;
                        assert(val != DEFAULT_FILL);
                    }
                } else if (val == DEFAULT_FILL) {
                    squareVectorRef<T>(vec->data(), size, i, j) =
                            fillFunction(squareVectorRef<T>(vec->data(), size,
                                                            i, i),
                                         squareVectorRef<T>(vec->data(), size,
                                                            j, j));
                }
            }
        }
    }

    /*! \brief Copy SquareVector onto another SquareVector with a different
     *         size
     *
     * \param other Reference to old SquareVector
     * \param oldSize Number of Rows/Columns of old SquareVector
     * \param newSize Number of Rows/Columns of new SquareVector
     *
     * \returns New SquareVector
     *
     * This Function copies a SquareVector and gives it a new size.
     */
    template <class T>
    vector<T> copyToSize(vector<T> &other, int oldSize, int newSize) {
        vector<T> replacement(newSize*newSize, DEFAULT_FILL);
        int copyUpTo = fmin(oldSize, newSize);
        for (int i=0; i<copyUpTo; i++) {
            for (int j=0; j<copyUpTo; j++) {
                squareVectorRef<T>(replacement.data(), newSize, i, j) =
                            squareVectorItem<T>(other.data(), oldSize, i, j);
            }
        }
        return replacement;
    }
}

//template <class T>
//class SquareVector{
//    public:
//        vector<T> vals;
//        int size;
//
//        SquareVector(int n) : size(n) {
//            vals = vector<T>(size*size, DEFAULT_FILL);
//
//        }
//        T &operator () (int row, int col) {
//            return vals[row*size + col];
//        }
//        void populate(function<T (T, T)> fillFunction) {
//            for (int i=0; i<size; i++) {
//                for (int j=0; j<size; j++) {
//                    T val = (*this)(i, j);
//                    if (i==j) {
//                        if (val == DEFAULT_FILL) {
//                            cout << "You have not defined interaction parameters for atom type with index " << i << endl;
//                            assert(val != DEFAULT_FILL);
//                        }
//                    } else if (val == DEFAULT_FILL) {
//                    (*this)(i, j) = fillFunction((*this)(i, i), (*this)(j, j));
//                    }
//                }
//            }
//        }
//        void setSize(int n) {
//            vector<T> old = vals;
//            if (n != size) {
//                int sizeOld = size;
//                size = n;
//                vals = vector<T>(size*size, DEFAULT_FILL);
//                int copyUpTo = fmin(size, sizeOld);
//                for (int i=0; i<copyUpTo; i++) {
//                    for (int j=0; j<copyUpTo; j++) {
//                        (*this)(i, j) = squareArrayItem<T>(old.data(), sizeOld, i, j);
//                    }
//                }
//            }
//        }
//        int totalSize() {
//            return size*size;
//        }
//};

/*! \class FixPair
 * \brief Fix for pair interactions
 *
 * This fix is the parent class for all types of pair interaction fixes.
 */
class FixPair : public Fix {

    public:

        /*! \brief Constructor */
        FixPair(SHARED(State) state_, string handle_, string groupHandle_,
                string type_, int applyEvery_)
            : Fix(state_, handle_, groupHandle_, type_, applyEvery_)
            {
                // Empty constructor
            };

    protected:

        /*! \brief Add a label to a given array
         *
         * \param label String specifying the label
         * \param arr Reference of array to which the label applies
         *
         * This function adds a label to a given GPUArray. After this step, the
         * array can be accessed using its label.
         */
        void labelArray(string label, GPUArray<float> &arr) {
            paramMap[label] = &arr;
        }

        /*! \brief Initialize the parameters
         *
         * \param paramHandle String containing the parameter handle
         * \param params Reference to GPUArray which will store the parameters
         *
         * This function can be used to set the parameters of the pair
         * interaction fix.
         */
        void initializeParameters(string paramHandle, GPUArray<float> &params);

        /*! \brief Fill the vector containing the pair interaction parameters
         *
         * \param array Reference to GPUArray which will store the parameters
         * \param fillFunction Function to calculate the interaction parameters
         *
         * This function prepares the parameter array used in the GPU
         * calculations
         */
        void prepareParameters(GPUArray<float> &array,
                            std::function<float (float, float)> fillFunction);

        /*! \brief Send parameters to all GPU devices */
        void sendAllToDevice();

        /*! \brief Make sure GPUArray storing the parameters has the right size
         *
         * \param array Reference to GPUArray storing the interaction
         *              parameters
         *
         * This function checks whether the GPUArray storing the interaction
         * parameters has the right size. If not, the array is automatically
         * resized.
         */
        void ensureParamSize(GPUArray<float> &array);

        /*! \brief Read pair parameters from XML node (Not yet implemented)
         *
         * \param xmlNode Node to read the parameters from
         *
         * \returns True if parameters have been read successfully and False
         *          otherwise.
         *
         * \todo Implement function reading pair parameters from XML node.
         *
         * This function reads the pair parameters from a given xmlNode.
         */
        bool readPairParams(pugi::xml_node xmlNode);

        /*! \brief Create restart chunk for pair parameters
         *
         * \param format Unused parameter
         *
         * \returns String containing the pair parameters for the different
         *          particle types.
         *
         * This function creates a restart chunk from the internally stored
         * pair parameter map. The chunk is the used for outputting the current
         * configuration.
         */
        string restartChunkPairParams(string format);

        /*! \brief Map mapping string labels onto the GPUArrays containing the
         *         pair potential parameters
         */
        map<string, GPUArray<float> *> paramMap;

    public:
        /*! \brief Set a specific parameter for specific particle types
         *
         * \param param String specifying the parameter to set
         * \param handleA String specifying first atom type
         * \param handleB String specigying second atom type
         * \param val Value of the parameter
         *
         * This function sets a specific parameter for the pair potential
         * between two atom types.
         */
        bool setParameter(string param, string handleA,
                          string handleB, double val);

//    GPUArrayDevice<float> copySqrToDevice(SquareVector<float> &vec) {
//        GPUArrayDevice<float> arr (vec.totalSize());
//        arr.set(vec.data());
//        return arr;
//    }
};
#endif
