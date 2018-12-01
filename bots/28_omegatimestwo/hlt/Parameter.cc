#include "Parameter.h"

vector<ParameterBase*> ParameterBase::param_registry;

void ParameterBase::parse_all(int argc, char **argv)
{
    for (ParameterBase *p : param_registry) {
        p->parse(argc, argv);
    }
}

