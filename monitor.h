#ifndef MONITOR_H_
#define MONITOR_H_

#include <simgrid/s4u.hpp>
void monitor(simgrid::s4u::Host *host,bool *busy);
const double computing_threshold=0.5;

#endif
