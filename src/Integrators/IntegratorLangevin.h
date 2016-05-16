#ifndef INTEGRATORLANGEVIN_H
#define INTEGRATORLANGEVIN_H


#include "IntegratorVerlet.h"
#include "Bounds.h"
#include "GPUArrayGlobal.h"
void export_IntegratorLangevin();

/*! \class IntegratorLangevin
 * \brief Langevin dynamics integrator
 *
 * Two step Langevin dynamics 
 */

class IntegratorLangevin : public IntegratorVerlet {

    public:
        /*! \brief Constant temperature Constructor */
        IntegratorLangevin(SHARED(State),/*string groupHandle_,*/float T_);

        /*! \brief Thermostat Constructor python list */
        IntegratorLangevin(SHARED(State), /*string groupHandle_,*/ boost::python::list intervals, boost::python::list temps, SHARED(Bounds) thermoBounds_ = SHARED(Bounds)(NULL));
        /*! \brief Thermostat Constructor */
        IntegratorLangevin(SHARED(State), /*string groupHandle_,*/ vector<double> intervals, vector<double> temps, SHARED(Bounds) thermoBounds_ = SHARED(Bounds)(NULL));
        void run(int);
        
        /*! \brief set the parameters
         *
         * \param seed for BD force
         * \param T temperature
         * \param gamma drag coeffictient
         *
         */
        void set_params(
            int seed_,//default 0
            float gamma_//default 1.0
                  ){
            seed=seed_;
            gamma=gamma_;
        }        
    private:
        int seed;
        float T;
        float gamma;
        //thermostat parameters
        vector<double> intervals;
        vector<double> temps;
        int curInterval;
        double curTemperature();
        bool finished;

        //bounds parameters
        SHARED(Bounds) thermoBounds;   
        bool usingBounds;
        BoundsGPU boundsGPU;        

        
        //         unsigned int groupTag;
        void preForce(uint);        
        void postForce(uint,int);/*! \todo handle 2D case  */
  
        //debug
        GPUArrayGlobal<float> VDotV;
        
};
#endif