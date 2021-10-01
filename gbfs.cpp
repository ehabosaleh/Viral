/*
 * gbfs.cpp
 *
 *  Created on: 24-Sep-2021
 *      Author: ehabosaleh
 */

#include<iostream>
#include<string>
#include<vector>
#include <map>
using namespace std;
#include <algorithm>
#include <bits/stdc++.h>
int getIndex(vector<long> v, int k)
{
    auto it = find(v.begin(), v.end(), k);
    // If element was found
    if (it != v.end())
    {
        int index = it - v.begin();
        return index;
    }
    else {
    	return -1;
    }
}
int getIndex(vector<string> v, string k)
{
    auto it = find(v.begin(), v.end(), k);
    // If element was found
    if (it != v.end())
    {
        int index = it - v.begin();
        return index;
    }
    else {
    	return -1;
    }
}
vector<string> gbfs(std::string source,std::string destination,
		std::map<std::string,vector<long>> cost,std::map<std::string,std::vector<std::string>> netzone)
{
bool found=false;
std::vector<long> total_cost;
std::vector<string> path;
std::vector<std::vector<string>> optimal_path;
std::vector<string> queue;
queue.push_back(source);
total_cost.push_back(0);
optimal_path.push_back({source});
vector<string> keys;
for(map<std::string,std::vector<std::string>>::iterator it = netzone.begin(); it != netzone.end(); ++it)
{
	keys.push_back(it->first);
}

string nominated_node;
int pos=0;
while(queue.size()!=0){

	long k=*min_element(total_cost.begin(), total_cost.end());
    int index = getIndex(total_cost,k);
	nominated_node=queue[index];
    if(nominated_node==destination)
    {
      found=true;
       pos=index;
       path.push_back(queue[getIndex(queue,nominated_node)]);
       queue.erase(queue.begin()+getIndex(queue,nominated_node));
    	break;
}
    else if(std::find(keys.begin(), keys.end(), nominated_node) != keys.end()){
    	string v=nominated_node;
        path.push_back(queue[getIndex(queue,nominated_node)]);
        queue.erase(queue.begin()+getIndex(queue,nominated_node));
        auto p=optimal_path[index];
        optimal_path.erase(optimal_path.begin()+index);
        total_cost.erase(total_cost.begin()+index);
        int counter=0;
        for(auto l :netzone[v]){
        	if((std::find(path.begin(), path.end(), l) >= path.end())&&	(std::find(queue.begin(), queue.end(), l) >= queue.end())) {
        		queue.push_back(l);
        		total_cost.push_back(cost[l][counter]);
        		auto p_temp=p;
        		p_temp.push_back(l);
        		optimal_path.push_back(p_temp);
        		counter+=1;
        	}
        	}
        }
        else{
        	path.push_back(queue[getIndex(queue,nominated_node)]);
           queue.erase(queue.begin()+getIndex(queue,nominated_node));

        optimal_path.erase(optimal_path.begin()+index);
        total_cost.erase(total_cost.begin()+index);
    }}
    if(found==true){return optimal_path[pos];}
    else{return {""};}
}


