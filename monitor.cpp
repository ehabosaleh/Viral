/*
 * monitor.cpp
 *
 *  Created on: 16-Sep-2021
 *      Author: ehabosaleh
 */
#include<simgrid/s4u.hpp>
#include"monitor.h"
#include<iostream>
#include<string>
using namespace std;
void monitor(simgrid::s4u::Host *host,bool *busy){
		int counter=0;
		while(true){
			if (host->get_available_speed()<=computing_threshold) {counter++;}
			else {counter=0;*busy=false;}
			simgrid::s4u::this_actor::sleep_for(300);
			simgrid::s4u::this_actor::get_host()->set_property("idleness_average", to_string(host->get_available_speed()));
			if(counter==1){*busy=true;counter=0;}// it will reach this point after 300 secs

		}
		}



