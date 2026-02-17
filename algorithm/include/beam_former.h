#ifndef BEAM_FORMER_H
#define BEAM_FORMER_H

#include "beam_former_template.h"

namespace vuprs
{
    class Beamformer_DCRCB: public vuprs::WidebandBeamformerTemplate
    {
        private:

            double steeringErrorRadius;
        
        protected:

            void CalculateBeamformingForOneFreq(int freqIndex) override;

        public:

            Beamformer_DCRCB();

            ~Beamformer_DCRCB();

            void SetSteeringErrorRadius(double r);
    };

    class Beamformer_CBF: public vuprs::WidebandBeamformerTemplate
    {
        protected:

            void CalculateBeamformingForOneFreq(int freqIndex) override;

        public:

            Beamformer_CBF();

            ~Beamformer_CBF();
    };
}

#endif
