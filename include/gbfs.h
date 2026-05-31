/*
 * gbfs.h
 *
 *  Created on: 29-Sep-2021
 *      Author: ehabosaleh
 */
#include<iostream>
#include<string>
#include<vector>
#include<map>
using namespace std;
#ifndef GBFS_H_
#define GBFS_H_

vector<string> gbfs(std::string source,std::string destination,
		std::map<std::string,vector<long>> cost,std::map<std::string,std::vector<std::string>> netzone);
int getIndex(vector<string> v, string k);
int getIndex(vector<long> v, int k);




#endif /* GBFS_H_ */
