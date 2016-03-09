#ifndef GPUARRAYDEVICE_H
#define GPUARRAYDEVICE_H

#include "cutils_math.h"
#include "globalDefs.h"
#include "memset_defs.h"

/*! \brief Global function to set the device memory */
void MEMSETFUNC(void *, void *, int, int);

/*! \class GPUArrayDevice
 * \brief Array on the GPU device
 *
 * \tparam T Data type stored in the array
 *
 * Array storing data on the GPU device.
 */
template <typename T>
class GPUArrayDevice {
    public:
        T *ptr; //!< Pointer to the data
        int n; //!< Number of entries stored in the device
        int Tsize; //!< Size (in bytes) of one array element

        /*! \brief Default constructor */
        GPUArrayDevice() {
            ptr = (T *) NULL;
            n = 0;
            Tsize = sizeof(T);
        }

        /*! \brief Constructor
         *
         * \param n_ Size of the array (number of elements)
         *
         * This constructor creates the array on the GPU device and allocates
         * enough memory to store n_ elements.
         */
        GPUArrayDevice(int n_) : n(n_) {
            allocate();
            Tsize = sizeof(T);
        }

        /*! \brief Allocate memory */
        void allocate() {
            CUCHECK(cudaMalloc(&ptr, n * sizeof(T)));
        }

        /*! \brief Deallocate memory */
        void deallocate() {
            if (ptr != (T *) NULL) {
                CUCHECK(cudaFree(ptr));
                ptr = (T *) NULL;
            }
        }

        /*! \brief Destructor */
        ~GPUArrayDevice() {
            deallocate();
        }

        /*! \brief Copy constructor */
        GPUArrayDevice(const GPUArrayDevice<T> &other) {
            n = other.n;
            allocate();
            CUCHECK(cudaMemcpy(ptr, other.ptr, n*sizeof(T),
                                                    cudaMemcpyDeviceToDevice));
            Tsize = sizeof(T);
        }

        /*! \brief Assignment operator */
        GPUArrayDevice<T> &operator=(const GPUArrayDevice<T> &other) {
            if (n != other.n) {
                deallocate();
                n = other.n;
                allocate();
            }
            CUCHECK(cudaMemcpy(ptr, other.ptr, n*sizeof(T),
                                                    cudaMemcpyDeviceToDevice));
            return *this;
        }

        /*! \brief Move constructor */
        GPUArrayDevice(GPUArrayDevice<T> &&other) {
            n = other.n;
            ptr = other.ptr;
            other.n = 0;
            other.ptr = (T *) NULL;
            Tsize = sizeof(T);
        }

        /*! \brief Move assignment operator */
        GPUArrayDevice<T> &operator=(GPUArrayDevice<T> &&other) {
            deallocate();
            n = other.n;
            ptr = other.ptr;
            other.n = 0;
            other.ptr = (T *) NULL;
            return *this;
        }

        /*! \brief Copy data to given pointer
         *
         * \param copyTo Pointer to the memory to where the data will be copied
         *
         * This function copies the data stored in the GPU array to the
         * position specified by the pointer *copyTo using cudaMemcpy
         */
        T *get(T *copyTo) {
            if (copyTo == (T *) NULL) {
                copyTo = (T *) malloc(n*sizeof(T));
            }
            CUCHECK(cudaMemcpy(copyTo, ptr, n*sizeof(T),
                                                    cudaMemcpyDeviceToHost));
            return copyTo;
        }

        /*! \brief Copy data to pointer asynchronously
         *
         * \param copyTo Pointer to the memory where the data will be copied to
         * \param stream cudaStream_t object used for asynchronous copy
         *
         * Copy data stored in the GPU array to the address specified by the
         * pointer copyTo using cudaMemcpyAsync.
         */
        T *getAsync(T *copyTo, cudaStream_t stream) {
            if (copyTo == (T *) NULL) {
                copyTo = (T *) malloc(n*sizeof(T));
            }
            CUCHECK(cudaMemcpyAsync(copyTo, ptr, n*sizeof(T),
                                            cudaMemcpyDeviceToHost, stream));
            return copyTo;
        }

        /*! \brief Copy data from pointer
         *
         * \param copyFrom Pointer to address from which the data will be
         *                 copied
         *
         * Copy data from a given adress specified by the copyFrom pointer to
         * the GPU array. The number of bytes copied from memory is the size of
         * the the GPUArrayDevice.
         */
        void set(T *copyFrom) {
            CUCHECK(cudaMemcpy(ptr, copyFrom, n*sizeof(T),
                                                    cudaMemcpyHostToDevice));
        }

        /*! \brief Copy data to pointer
         *
         * \param dest Pointer to the memory to which the data should be copied
         *
         * Copy data from the device to the memory specified by the pointer
         * dest.
         *
         * \todo This function is essential identical to get(). Can we have one
         * function that covers both cases (void* and T* pointers)?
         */
        void copyToDeviceArray(void *dest) {
            CUCHECK(cudaMemcpy(dest, ptr, n*sizeof(T),
                                                    cudaMemcpyDeviceToDevice));
        }

        /*! \brief Copy data to pointer asynchronously
         *
         * \param dest Pointer to the memory to which the data should be copied
         * \param stream cudaStream_t object for asynchronous copy
         *
         * Copy data from the device to the memory specified by the dest
         * pointer using cudaMemcpyAsync.
         *
         * \todo This function is essentially identical to getAsync(). Can we
         * have one function that covers both cases (void* and T* pointers)?
         */
        void copyToDeviceArrayAsync(void *dest, cudaStream_t stream) {
            CUCHECK(cudaMemcpyAsync(dest, ptr, n*sizeof(T),
                                            cudaMemcpyDeviceToDevice, stream));
        }

        /*! \brief Set all bytes in the array to a specific value
         *
         * \param val Value written to all bytes in the array
         *
         * WARNING: val must be a one byte value
         *
         * Set each byte in the array to the value specified by val.
         *
         * \todo For this function val needs to be converted to unsigned char
         * and this value is used.
         */
        void memset(T val) {
            CUCHECK(cudaMemset(ptr, val, n*sizeof(T)));
        }

        /*! \brief Set array elements to a specific value
         *
         * \param val Value the elements are set to
         *
         * Set all array elements to the value specified by the parameter val
         */
        void memsetByVal(T val) {
            assert(Tsize==4 or Tsize==8 or Tsize==12 or Tsize==16);
            MEMSETFUNC((void *) ptr, &val, n, Tsize);
        }
};

#endif
