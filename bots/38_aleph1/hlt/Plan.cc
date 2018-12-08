#include "Plan.h"

string desc(Purpose p)
{
    switch (p) {
        case Purpose::Stuck: return "Stuck"; 
        case Purpose::EvadeReturner: return "EvadeReturner"; 
        case Purpose::Return: return "Return"; 
        case Purpose::Mine: return "Mine";
        default: return "PURPOSE::ERROR";
    }
}