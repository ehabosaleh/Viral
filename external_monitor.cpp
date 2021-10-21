#include <simgrid/s4u.hpp>
#include "simgrid/plugins/energy.h"
#include "simgrid/plugins/load.h"
#include<iostream>
#include<fstream>
#include<ctime>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include "simgrid/kernel/routing/ClusterZone.hpp"
#include"simgrid/kernel/ProfileBuilder.hpp"
#include"monitor.h"
#include"gbfs.h"
namespace sg4 = simgrid::s4u;
using namespace std;
XBT_LOG_NEW_DEFAULT_CATEGORY(simple_example, "simple example");

struct Source{
  std::string source_name;
  string infected ;
  double computing_power;
  double RAM;
  double freemem;
  double residue;
  double speed;
  std::string super_node;
  uint64_t size;//in Bytes
};
struct Result{
	std::string source_name;
	std::string super_node;
	std::string participants;
	std::string result;
	double execution_time;
	bool completion;
	double residue=0;
	uint64_t size;};

struct Work{
	std::string source_name;
	std::string super_node;
	double work_per_host;
	uint64_t size;};

std::map<std::string,std::vector<std::string>> netzone;
std::map<std::string,std::vector<std::string>> two_direction_netzone;
std::map<std::string,vector<long>> cost;
std::map<std::string,std::map<std::string,std::vector<double>>> computing_tree;
std::vector<string> sources;
bool one_time_pad_work=true;
double work_to_do=10E12;
static void host_respond(sg4::Mailbox*,bool *);
static void inquire(bool,bool *,string);
static void get_info(sg4::Mailbox * ,Source *,bool, int*,bool *);
static void worker(sg4::Mailbox*);
static void work_distributor(double,bool);
static void wait_for_inquiry(bool *,Result * );
static void peers_connection_inquiring();
static void emad(string);
int main(int argc, char* argv[])
{
	sg_host_energy_plugin_init();
	sg4::Engine e(&argc, argv);
	e.load_platform(argv[1]);
	peers_connection_inquiring();
	bool *inquiring=new bool;
	*inquiring=true;
	sg4::Host * root_host=sg4::Host::by_name("host_0");//define the source...
	sg4::ActorPtr actor=sg4::Actor::create("inquiring_actor",root_host, inquire,true,inquiring,"");
    e.run();

    XBT_INFO("Simulation time %g Seconds", simgrid::s4u::Engine::get_clock());
    return 0;
}

static void aternative_sender(string sender_name,string receiver_name, Result result_packet){
	sg4::Mailbox::by_name(sender_name+'_'+receiver_name)->put(new Result( result_packet), result_packet.size);
	XBT_INFO("This is %s sending from %s",sg4::this_actor::get_host()->get_cname(),(sender_name+'_'+receiver_name).data());
}
static void alternative_receiver(string receiver_name,string sender_name ){
	auto * result=static_cast<Result *>(sg4::Mailbox::by_name((sender_name+'_'+receiver_name))->get());
	XBT_INFO("This is %s receiving from %s ",sg4::this_actor::get_host()->get_cname(),(sender_name+'_'+receiver_name).data());
//	aternative_sender( sender_name,*result);
}
static void alternative_sender_receiver(vector<string>path,Result result_packet){

	//aternative_sender(path[0],result_packet);
	for(int i=0;i<path.size()-1;i++){

		if(i!=path.size()-2){
		sg4::Actor::create("alternative_sender", sg4::Host::by_name(path[i]), aternative_sender,path[i],path[i+1],result_packet);
		sg4::Actor::create("alternative_receiver", sg4::Host::by_name(path[i+1]), alternative_receiver,path[i+1],path[i]);
		}
		else{
			sg4::Actor::create("alternative_sender", sg4::Host::by_name(path[i]), aternative_sender,path[i+1],path[0],result_packet);
			//sg4::Actor::create("alternative_receiver", sg4::Host::by_name(path[i+1]), alternative_receiver,path[0],path[i+1]);
		}
		}

}

static void peers_connection_inquiring(){
	netzone.clear();
	cost.clear();
	two_direction_netzone.clear();
	//computing_tree.clear();
	std::vector<simgrid::s4u::Link*> links;
	std::vector<simgrid::kernel::routing::ClusterZone*> clusters =sg4::Engine::get_instance()->get_filtered_netzones<simgrid::kernel::routing::ClusterZone>();
		for(auto cluster:clusters)
		 for(auto dst_host:cluster->get_all_hosts()){
			 auto src_host=cluster->get_all_hosts()[0];
				 if(src_host->get_name()==dst_host->get_name()){continue;}
				 else{
					 src_host->route_to(dst_host, links, 0);
					 if(links.size()>0){
						auto src=src_host->get_name();
						auto dst=dst_host->get_name();
						netzone[src].push_back(dst);
						XBT_INFO("Source name is %s. Destination name is %s. link name is %s",src.data(),dst.data(),links[links.size()-1]->get_name().data());
						}
					 }
					links.clear();
		 }


			std::vector<simgrid::s4u::Link*> all_links = sg4::Engine::get_instance()->get_all_links();
			std::vector<simgrid::s4u::Link*> link_s;

			for(int i=1;i<all_links.size();i++){
				if (all_links[i]->is_on()==false){
					auto src=all_links[i]->get_property("src");
					auto dst=all_links[i]->get_property("dst");
					computing_tree[src].erase(dst);
					continue;}
				else{
					auto src=all_links[i]->get_property("src");
					auto dst=all_links[i]->get_property("dst");
					sg4::Host *host_dst=sg4::Host::by_name(dst);
					sg4::Host *host_src=sg4::Host::by_name(src);
					host_src->route_to(host_dst, link_s, 0);
					auto s=host_src->get_name();
					auto d=host_dst->get_name();
					for(auto link : link_s){
						XBT_INFO("Rout from %s to host %s is %s",s.data(),d.data(),link->get_name().data());
					}
					netzone[src].push_back(dst);
					two_direction_netzone[src].push_back(dst);
					two_direction_netzone[dst].push_back(src);
					cost[src].push_back((long)(all_links[i]->get_latency()*1000000));
					cost[dst].push_back((long)(all_links[i]->get_latency()*1000000));
					link_s.clear();
				}
			}
			{
			/*
			 for(auto link:all_links){
				if(link->get_name()=="link_a_b"){
					auto src=link->get_property("src");
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_b_c"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_c_d"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_a_0"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_a_1"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_a_2"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_a_3"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_a_4"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}

				if(link->get_name()=="link_b_0"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_b_1"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_b_2"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_b_3"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_b_4"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_c_0"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_c_1"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_c_2"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_c_3"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_c_4"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_d_0"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_d_1"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_d_2"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_d_3"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}
				if(link->get_name()=="link_d_4"){
					auto src=link->get_property("src"); // using the link for direct connection in both directions
					auto dst=link->get_property("dst");
					netzone[src].push_back(dst);
					}

			 }
		*/
			}
}
static void wait_for_inquiry(bool *inquiring,Result *result_packet){
	//XBT_INFO("wait for inquiring...");
	while(true){

		sg4::this_actor::sleep_for(0.1);
	if(*inquiring==false){
		XBT_INFO("End of re-inquiring at source %s\n",result_packet->source_name.data());
		break;
	}
		}
	if(result_packet->source_name!=sources[0])
		sg4::ActorPtr actor=simgrid::s4u::Actor::create("new_source",sg4::Host::by_name(result_packet->source_name),work_distributor,result_packet->residue,false);
	else
		sg4::ActorPtr actor=simgrid::s4u::Actor::create("new_source",sg4::Host::by_name(result_packet->source_name),work_distributor,result_packet->residue,true);
	result_packet->residue=0;
	//result_packet->completion=true;

}

static void receive_outcome(sg4::Mailbox *mail_b,map<string,Work*> works_queue,string sender_name,Result * result_packet,bool root,int *i){
	//Result *result;
	try{

	 auto* result=static_cast<Result *>(mail_b->get());
	XBT_INFO("Receiving from %s result",mail_b->get_cname());
	result_packet->participants+= ","+result->participants;
	result_packet->result+=" "+result->result;
	result_packet->residue+=result->residue;
	*i+=1;
	XBT_INFO("I value is %d and size %d",*i,computing_tree[result_packet->source_name].size());
	XBT_INFO("Residue at %s is %f Mflops\n",result_packet->source_name.data(),result_packet->residue/10e6);

	 if(root==false && *i==computing_tree[result_packet->source_name].size()&&result_packet->residue==0){
		 string super_host_name;
		 for(map<string, map<string,vector<double >>>::iterator outer_iter=computing_tree.begin(); outer_iter!=computing_tree.end(); ++outer_iter)
		 for(map<string,vector<double>>::iterator inner_iter=outer_iter->second.begin(); inner_iter!=outer_iter->second.end(); ++inner_iter)
		 if(result_packet->source_name==inner_iter->first){
			  super_host_name=outer_iter->first;
			  break;
		 }

		try{
			XBT_INFO("Trying to send forth up the final result %f",sg4::Engine::get_clock());
		sg4::Mailbox::by_name(super_host_name+"_"+result_packet->source_name)->put(result_packet, result_packet->size);
		XBT_INFO("Work completed at node %s. send forth up\n",result_packet->source_name.data());
		}
		catch(const simgrid::NetworkFailureException&){
			XBT_INFO("Cannot send result from sub worker %s to %s",result_packet->source_name.data(),super_host_name.data());
			std::vector<simgrid::s4u::Link*> all_links = sg4::Engine::get_instance()->get_all_links();
			peers_connection_inquiring();
			vector<string> path= gbfs(result_packet->source_name, super_host_name,cost,two_direction_netzone);
			try{
			if(path.empty()==false){
			XBT_INFO("The alternative path is:");
			for(auto s: path){XBT_INFO("%s",s.data());}
			alternative_sender_receiver(path,*result_packet);
			}
			else{mail_b->put(new Result( *result_packet), result_packet->size);}
			}

		catch(...){
				XBT_INFO("There is no other path between %s and %s",result_packet->source_name.data(),super_host_name.data());
		}
		}
	 }
	else if(*i==computing_tree[result_packet->source_name].size()&&result_packet->residue>0){
		bool *inquiring=new bool;
		*inquiring=true;
		XBT_INFO("Work did not complete at %s. Let's redistribute the residue",result_packet->source_name.data());
		XBT_INFO("Results so far  are :\n %s",result_packet->result.data());
		sg4::ActorPtr actor=sg4::Actor::create("actor_0",sg4::Host::by_name(result_packet->source_name), inquire,true,inquiring,"Does not matter");// true because we update part of the computing tree not all the computing tree
		sg4::Actor::create("Waiting For Inquiry Procedure",sg4::Host::by_name(result_packet->source_name), wait_for_inquiry,inquiring, result_packet);
		}

	else if(root==true && *i==computing_tree[result_packet->source_name].size()){
		XBT_INFO("This is the source %s",result_packet->source_name.data());
		XBT_INFO("Total participant hosts are:%s",result_packet->participants.data());
		XBT_INFO("Final Result is :\n %s",result_packet->result.data());
		XBT_INFO("Total Residue is:%f Mflops",result_packet->residue/1e6);
		//for(host in )
	}}

	catch(const simgrid::NetworkFailureException&){
		XBT_INFO("%s Cannot receive result via link: %s",result_packet->source_name.data(),mail_b->get_cname());
		try{
			sg4::this_actor::sleep_for(1);
			auto * result=static_cast<Result *>(mail_b->get());
				XBT_INFO("Receiving from %s result",mail_b->get_cname());
				result_packet->participants+= ","+result->participants;
				result_packet->result+=" "+result->result;
				result_packet->residue+=result->residue;
				XBT_INFO("Residue at %s is %f Mflops\n",result_packet->source_name.data(),result_packet->residue/10e6);

				if(root==false && *i==computing_tree[result_packet->source_name].size()&&result_packet->residue==0){
					 string super_host_name;
					 for(map<string, map<string,vector<double >>>::iterator outer_iter=computing_tree.begin(); outer_iter!=computing_tree.end(); ++outer_iter)
					 for(map<string,vector<double>>::iterator inner_iter=outer_iter->second.begin(); inner_iter!=outer_iter->second.end(); ++inner_iter)
					 if(result_packet->source_name==inner_iter->first){
						  super_host_name=outer_iter->first;
						  break;
					 }

					sg4::Mailbox::by_name(super_host_name+"_"+result_packet->source_name)->put(result_packet, result_packet->size);
					XBT_INFO("Work completed at node %s. send forth up\n",result_packet->source_name.data());
					}

				else if(*i==computing_tree[result_packet->source_name].size()&&result_packet->residue>0){
					bool *inquiring=new bool;
					*inquiring=true;
					XBT_INFO("Work did not complete at %s. Let's redistribute the residue",result_packet->source_name.data());
					XBT_INFO("Results so far  are :\n %s",result_packet->result.data());
					sg4::ActorPtr actor=sg4::Actor::create("actor_0",sg4::Host::by_name(result_packet->source_name), inquire,true,inquiring,"Does not matter");// true because we update part of the computing tree not all the computing tree
					sg4::Actor::create("Waiting For Inquiry Procedure",sg4::Host::by_name(result_packet->source_name), wait_for_inquiry,inquiring, result_packet);
					}

				else if(root==true && *i==computing_tree[result_packet->source_name].size()){
					XBT_INFO("This is the source %s",result_packet->source_name.data());
					XBT_INFO("Total participant hosts are:%s",result_packet->participants.data());
					XBT_INFO("Final Result is :\n %s",result_packet->result.data());
					XBT_INFO("Total Residue is:%f Mflops",result_packet->residue/1e6);
					//for(host in )
				}
				//*i+=1;

			}
	catch(const simgrid::NetworkFailureException&){
		XBT_INFO("Connection was lost with %s...need to redistribute the work again ...",sender_name.data());
		result_packet->residue+=works_queue[sender_name]->work_per_host;
		XBT_INFO("Residue at %s is %f Mflops\n",result_packet->source_name.data(),result_packet->residue/1e6);
		//*i+=1;
		if(*i==computing_tree[result_packet->source_name].size()&&result_packet->residue>0){
			bool *inquiring=new bool;
			*inquiring=true;
			XBT_INFO("Work did not complete at %s. Let's redistribute the residue",result_packet->source_name.data());
			XBT_INFO("Results so far  are :\n %s",result_packet->result.data());
			sg4::ActorPtr actor=sg4::Actor::create("actor_0",sg4::Host::by_name(result_packet->source_name), inquire,true,inquiring,"Does not matter");// true because we update part of the computing tree not all the computing tree
			sg4::Actor::create("Waiting For Inquiry Procedure",sg4::Host::by_name(result_packet->source_name), wait_for_inquiry,inquiring, result_packet);
			}
		//*i+=1;
			}


		}

		}



static void local_worker(double work,Result* result_packet){
	bool * busy=new bool;
	*busy=false;
	double start=sg4::Engine::get_clock();
	string original_file_name="availability/"+sg4::this_actor::get_host()->get_name()+"_availability.txt";
	string temporary_file_name="availability/"+sg4::this_actor::get_host()->get_name()+"_availability_temp.txt";
	ofstream original_file(original_file_name);// file to write
	ifstream temporary_file(temporary_file_name);// file to read
	string start_point=to_string(int(floor(start)))+" "+to_string(0)+"\n";
	int currentLine=0;
	string strTemp_0,strTemp_1;
    while (temporary_file >> strTemp_0>>strTemp_1 )
    {
    	if(currentLine==floor(start))
    	{original_file<<start_point;}
    	else{
    				original_file<<strTemp_0<<" "<<strTemp_1<<"\n";
    	}
         ++currentLine ;
    }
    temporary_file.close();
    original_file.close();

	//sg4::this_actor::execute(work);//execution without monitoring the CPU power;
	sg4::ActorPtr supervisor=sg4::Actor::create("monitor", sg4::this_actor::get_host(), monitor,sg4::this_actor::get_host(),busy);
	double new_work;
	while(true){
	new_work=sg4::this_actor::get_host()->get_available_speed()*sg4::this_actor::get_host()->get_speed();
	sg4::this_actor::execute(new_work);
	work=work-new_work;
	if(work<new_work){sg4::this_actor::execute(work);
	supervisor->kill();
	result_packet->completion=true;break;}
	else if (*busy==true){
			XBT_INFO("Host  %s was suspended at speed %f",sg4::this_actor::get_host()->get_cname(),sg4::this_actor::get_host()->get_available_speed());
			sg4::this_actor::sleep_for(300);
			if (*busy==false){
				XBT_INFO("Host %s can return to work now",sg4::this_actor::get_host()->get_cname());
				continue;}
			else{
				XBT_INFO("Host %s is busy now and cannot do the work",sg4::this_actor::get_host()->get_cname());
				supervisor->kill();
				result_packet->completion=false;
				result_packet->residue=work;
				break;}}}


	result_packet->execution_time=sg4::Engine::get_clock()-start;
	XBT_INFO("Work is done in %s; Duration: %f Seconds",sg4::this_actor::get_host()->get_cname(),(sg4::Engine::get_clock()-start));
	result_packet->result=sg4::this_actor::get_host()->get_name()+ " completed work in "+to_string(sg4::Engine::get_clock()-start)+" Sec\n";
	result_packet->participants=sg4::this_actor::get_host()->get_cname();

	string original_file_name_1="availability/"+sg4::this_actor::get_host()->get_name()+"_availability.txt";
	string temporary_file_name_1="availability/"+sg4::this_actor::get_host()->get_name()+"_availability_temp.txt";
	ifstream original_file_1(original_file_name_1);// file to write
	ofstream temporary_file_1(temporary_file_name_1);// file to read
	currentLine=1;
	string strT_2,strT_3;
	while (original_file_1 >> strT_2>>strT_3 )
	    {
		temporary_file_1<<strT_2<<" "<<strT_3<<"\n";
	      ++currentLine ;
	    }
	    temporary_file_1.close();
	    original_file_1.close();
	sg4::this_actor::exit();

}

static void worker(sg4::Mailbox * mail_b){
		Result result_packet;
		bool * busy=new bool;
		*busy=false;
		//sg4::Mailbox *mail_b=sg4::Mailbox::by_name(sg4::this_actor::get_host()->get_cname());
		auto *work_packet=	static_cast<Work *>(mail_b->get());
		double work=work_packet->work_per_host;
		double new_work;
		if(computing_tree[sg4::this_actor::get_host()->get_cname()].empty()==true){
			double start=sg4::Engine::get_clock();
			//XBT_INFO("\n\nHost name is: %s\n\n",sg4::this_actor::get_host()->get_cname());
			string original_file_name="availability/"+sg4::this_actor::get_host()->get_name()+"_availability.txt";
			string temporary_file_name="availability/"+sg4::this_actor::get_host()->get_name()+"_availability_temp.txt";
			ofstream original_file(original_file_name);// file to write
			ifstream temporary_file(temporary_file_name);// file to read
			string start_point=to_string(int(floor(start)))+" "+to_string(0)+"\n";
			int currentLine=0;
			string strTemp_0,strTemp_1;
		    while (temporary_file >> strTemp_0>>strTemp_1 )
		    {
		    	if(currentLine==floor(start))
		    	{original_file<<start_point;}
		    	else{

		    				original_file<<strTemp_0<<" "<<strTemp_1<<"\n";
		    	}
		         ++currentLine ;
		    }
		    temporary_file.close();
		    original_file.close();


			//sg4::this_actor::execute(*work);//execution without monitoring the cpu power;
			sg4::ActorPtr supervisor=sg4::Actor::create("monitor", sg4::this_actor::get_host(), monitor,sg4::this_actor::get_host(),busy);
			while(true){
			new_work=sg4::this_actor::get_host()->get_available_speed()*sg4::this_actor::get_host()->get_speed();
			sg4::this_actor::execute(new_work);
			work=work-new_work;
			if(work<new_work){sg4::this_actor::execute(work);supervisor->kill();result_packet.completion=true;result_packet.residue=0;break;}
			if (*busy==true){// the condition should monitor the cpu power for long time
				XBT_INFO("Host %s was suspended at speed %f",sg4::this_actor::get_host()->get_cname(),sg4::this_actor::get_host()->get_available_speed());
				sg4::this_actor::sleep_for(300);
				if (*busy==false){
					XBT_INFO("Host %s can return to work now",sg4::this_actor::get_host()->get_cname());
					continue;}
				else{
				XBT_INFO("Host %s is busy now and cannot do the work",sg4::this_actor::get_host()->get_cname());
				supervisor->kill();
				result_packet.completion=false;
				result_packet.residue=work;
				break;
				}
				}
			}

			result_packet.execution_time=sg4::Engine::get_clock()-start;
			XBT_INFO("Work is done in %s; Duration: %f Seconds",sg4::this_actor::get_host()->get_cname(),(sg4::Engine::get_clock()-start));
			result_packet.result=sg4::this_actor::get_host()->get_name()+ " completed work in "+to_string(sg4::Engine::get_clock()-start)+" Sec\n";
			result_packet.source_name=sg4::this_actor::get_host()->get_cname();
			result_packet.participants=sg4::this_actor::get_host()->get_cname();
			result_packet.size=1024;


				string original_file_name_1="availability/"+sg4::this_actor::get_host()->get_name()+"_availability.txt";
				string temporary_file_name_1="availability/"+sg4::this_actor::get_host()->get_name()+"_availability_temp.txt";
				ifstream original_file_1(original_file_name_1);// file to write
				ofstream temporary_file_1(temporary_file_name_1);// file to read
				currentLine=1;
				string strT_2,strT_3;
				while (original_file_1 >> strT_2>>strT_3 )
				    {
					temporary_file_1<<strT_2<<" "<<strT_3<<"\n";
				      ++currentLine ;
				    }
				    temporary_file_1.close();
				    original_file_1.close();

			try{
				XBT_INFO("Trying to send result from sub worker%s to super worker",sg4::this_actor::get_host()->get_cname());
				mail_b->put(new Result( result_packet), result_packet.size);
			}
			catch(simgrid::NetworkFailureException& e){
				XBT_INFO("Cannot send result from sub worker %s to %s",sg4::this_actor::get_host()->get_cname(),work_packet->source_name.data());
				std::vector<simgrid::s4u::Link*> all_links = sg4::Engine::get_instance()->get_all_links();
				peers_connection_inquiring();
				vector<string> path= gbfs(sg4::this_actor::get_host()->get_cname(), work_packet->source_name,cost,two_direction_netzone);
				try{
				if(path.empty()==false){
				XBT_INFO("The alternative path is:");
				for(auto s: path){XBT_INFO("%s",s.data());}
				alternative_sender_receiver(path,result_packet);
				}
				else{mail_b->put(new Result( result_packet), result_packet.size);}
				}

			catch(...){
					XBT_INFO("There is no other path between %s and %s",sg4::this_actor::get_host()->get_cname(),work_packet->source_name.data());
			}

				}
			}
		else{
			simgrid::s4u::Actor::create("new_source",simgrid::s4u::Host::by_name(sg4::this_actor::get_host()->get_cname()),work_distributor,work,false);
			}
		sg4::this_actor::exit();
			}

static void work_distributor(double work,bool root){
	Result * result_packet=new Result();
	Work * work_0=new Work();
	std::map<string,Work*> works_queue;
	work_0->source_name=sg4::this_actor::get_host()->get_name();
	int *i=new int();
	std::string source_name=sg4::this_actor::get_host()->get_cname();
	result_packet->source_name=source_name;
	result_packet->residue=0;
	simgrid::s4u::Host *source=simgrid::s4u::Host::by_name(source_name);
	//sg4::ActorPtr supervisor=sg4::Actor::create("monitor", sg4::this_actor::get_host(), monitor,sg4::this_actor::get_host(),busy);
	double total_RAM_size=0;
	double total_computing_power=0;
	double available_speed=stod(source->get_property("idleness_average"));
	if(available_speed>computing_threshold){
	 total_RAM_size=stod(source->get_property("RAM"));
	 total_computing_power=available_speed;}
	for (auto host_name:computing_tree[source_name])
	{
		total_computing_power+=computing_tree[source_name][host_name.first][0];//{[0],[1]}===>{[computing_power],[ram size]}
		total_RAM_size+=computing_tree[source_name][host_name.first][1];
	}
	double work_per_FLOP=work/total_computing_power;
	double work_per_host=0.0;

	if(available_speed>computing_threshold){
	sg4::Actor::create("local_worker",source, local_worker,(work_per_FLOP*available_speed),result_packet);
	}
	std::vector<simgrid::s4u::Mailbox * >mail_box={};
	for(auto host:computing_tree[source_name]){			// 1)create new actors+ create new mail boxes named as new workers
			//work_per_host=work_per_FLOP*computing_tree[source_name][host.first][0];
			work_0->work_per_host=work_per_FLOP*computing_tree[source_name][host.first][0];
			mail_box.push_back(simgrid::s4u::Mailbox::by_name(source_name+'_'+host.first));
			works_queue[host.first]=work_0;
		//	XBT_INFO("work per hot is%f",works_queue[host.first]->work_per_host);
			simgrid::s4u::Host *new_worker=simgrid::s4u::Host::by_name(host.first);
			sg4::Actor::create("new_worker",sg4::Host::by_name(host.first), worker,simgrid::s4u::Mailbox::by_name(source_name+'_'+host.first));
			sg4::Mailbox::by_name(source_name+'_'+host.first)->put(work_0, sizeof(work_0));
			XBT_INFO("Send %f Mflops from %s to %s\n",work_0->work_per_host/1e6,source_name.data(),new_worker->get_cname());
		}

	//for(auto mail_b:mail_box){
		for(auto host:computing_tree[source_name]){
				sg4::Mailbox *mail_b=sg4::Mailbox::by_name(source_name+'_'+host.first);
				sg4::Actor::create("outcomes_receiver",source, receive_outcome,mail_b,works_queue,host.first,result_packet,root,i);
								}
	sg4::this_actor::exit();
	}


static void get_info(sg4::Mailbox * mail_b,Source* the_source,bool root, int *i,bool*inquiring){
	auto *host_specification = static_cast<Source *>(mail_b->get());// get from put in respond() function
	XBT_INFO("Receiving responses via the link %s...",mail_b->get_cname());
	XBT_INFO("Receiving %f of free RAM memory",host_specification->freemem);
	XBT_INFO("Receiving %f of computing power\n",host_specification->computing_power);
	bool ignore=false;
	if(host_specification->computing_power>computing_threshold)
	  {
			computing_tree[the_source->source_name][host_specification->source_name]={host_specification->computing_power,host_specification->freemem};
			int counter_0=0;
			int counter_1=0;
			for(map<string, map<string,vector<double >>>::iterator outer_iter=computing_tree.begin(); outer_iter!=computing_tree.end(); ++outer_iter)
				for(map<string,vector<double>>::iterator inner_iter=outer_iter->second.begin(); inner_iter!=outer_iter->second.end(); ++inner_iter){
					if(host_specification->source_name==inner_iter->first){counter_0++;}
					if(counter_0==2){computing_tree[the_source->source_name].erase(host_specification->source_name);
					ignore=true;
					XBT_INFO("Cannot add %s to %s. It was added before ",host_specification->source_name.data(),the_source->source_name.data());
					break;}
				}
			if(ignore==false){
			the_source->RAM+=host_specification->RAM;
			the_source->freemem+=host_specification->freemem;
			the_source->computing_power+=host_specification->computing_power;
			the_source->speed+=host_specification->speed;
			the_source->infected+=host_specification->source_name+" , "+host_specification->infected;
			the_source->size+=host_specification->size;
			}
			}
	else {
		computing_tree[the_source->source_name].erase(host_specification->source_name);
		XBT_INFO("Cannot add %s to %s. Low Computing Power",host_specification->source_name.data(),the_source->source_name.data());}


	*i+=1;
	if(root==false && *i==netzone[the_source->source_name].size()){
		XBT_INFO("Send information to the super node %s",(the_source->super_node).data());
		sg4::Mailbox::by_name(the_source->super_node+'_'+the_source->source_name)->put(the_source,the_source->size);
			}

if(root==true && *i==netzone[the_source->source_name].size()){
	XBT_INFO("This is the source %s",the_source->source_name.data());
	XBT_INFO("Total free random memory size is:  %f",the_source->RAM);
	XBT_INFO("CPUs' total power is:  %f",the_source->computing_power);
	XBT_INFO("Total infected hosts are:%s\n",the_source->infected.data());
	*inquiring=false;

	if(one_time_pad_work==true){
		sg4::Host * root_host=sg4::Host::by_name("host_0");//define the source...
		sg4::Actor::create("actor_0",root_host, work_distributor,work_to_do,true);
		one_time_pad_work=false;
	}

}
//sg4::this_actor::exit();
	}


static void inquire(bool root,bool *inquiring,string inquirer)
{
	int *i=new int();
	XBT_INFO("The source %s is inquiring about the computing capability of all other connected nodes",sg4::this_actor::get_host()->get_cname());
	Source * the_source=new Source();
	string host_name=simgrid::s4u::this_actor::get_host()->get_cname();
	the_source->source_name=host_name;
	the_source->super_node=inquirer;//////////
	simgrid::s4u::Host *host=simgrid::s4u::Host::by_name(host_name);
	if(stod(host->get_property("idleness_average"))>computing_threshold){
		the_source->RAM=stod(host->get_property("RAM"));
		the_source->freemem=stod(host->get_property("freemem_average"));
		the_source->speed=host->get_speed();
		the_source->computing_power=stod(host->get_property("idleness_average"));

	}

	sources.push_back(the_source->source_name);

	vector<string> message={the_source->source_name,"send_info"};
	std::vector<simgrid::s4u::Mailbox * >mail_box={};

	for(auto host:netzone[host_name]){					// 1)create new actors+ create new mail boxes named as new actors
	mail_box.push_back(simgrid::s4u::Mailbox::by_name(host_name+'_'+host));
	simgrid::s4u::Host *new_host=simgrid::s4u::Host::by_name(host);
	simgrid::s4u::Actor::create("respondent", new_host, host_respond,simgrid::s4u::Mailbox::by_name(host_name+'_'+host),inquiring);
}

for(auto mail_b:mail_box){
	mail_b->put(new std::vector<string>(message), message.size());                  // 2) send the request from the source node
//	XBT_INFO("Request has been sent: %s.\n",mail_b->get_cname());

}

for(auto mail_b:mail_box){ 				// 3)receive the response from leaf nodes
//###### ######
		sg4::Actor::create("listener",sg4::Host::by_name(host_name) , get_info,mail_b,the_source,root,i,inquiring);
}

sg4::this_actor::exit();
}

static void host_respond(sg4::Mailbox *mail_b ,bool *inquiring)
{

	 auto *msg = static_cast<vector<string>*>(mail_b->get());

	 string request=msg->data()[1];
	 if(request=="send_info"){

	// XBT_INFO("Request has been received to: %s\n",sg4::this_actor::get_host()->get_cname());
	 Source host={};
	 if(netzone[sg4::this_actor::get_host()->get_cname()].empty()==true){

		 host.RAM=stod(sg4::this_actor::get_host()->get_property("RAM"));
		 host.freemem=stod(sg4::this_actor::get_host()->get_property("freemem_average"));
		 host.computing_power=stod(sg4::this_actor::get_host()->get_property("idleness_average"));
		 host.speed=sg4::this_actor::get_host()->get_speed();
		 host.source_name=sg4::this_actor::get_host()->get_cname();
		 host.size=1024;
		 mail_b->put(new Source(host), host.size);//send from leaf node;


	 }
	else{

		simgrid::s4u::Actor::create("new_source",simgrid::s4u::Host::by_name(sg4::this_actor::get_host()->get_cname()),inquire,false,inquiring,msg->data()[0]);}

	 }
	 sg4::this_actor::exit();
	 }
