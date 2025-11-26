#ifndef BEAM_FORMER_H
#define BEAM_FORMER_H

#include "beam_former_template.h"

namespace vuprs
{
    class BeamFormerCBF: public BeamFormerTemplate
    {
        public:

            void OutputSignal(std::vector<std::complex<double>> *outputSignal) override;
    };
}

#endif
