#include <simgrid/s4u.hpp>
#include "simgrid/plugins/energy.h"
#include<iostream>
#include<ctime>
#include <stdio.h>
#include <stdlib.h>
namespace sg4 = simgrid::s4u;
using namespace std;
XBT_LOG_NEW_DEFAULT_CATEGORY(s4u_actor_migration, "Messages specific for this s4u example");

struct Host{
 std::string host_name;
  double computing_power;
  double RAM;
  uint64_t size;//in Bytes
};

struct Source{
  std::string source_name;
  string infected ;
  double computing_power;
  double RAM;
  double residue;
  uint64_t size;//in Bytes

};
struct Result{
	std::string source_name;
	std::string participants;
	std::string result;
	double execution_time;
	bool completion;
	double residue=0;
	uint64_t size;};

std::map<std::string,std::vector<std::string>> netzone;
std::map<std::string,std::map<std::string,std::vector<double>>> computing_tree;
std::vector<string> sources;
bool one_time_pad_work=true;

static void host_respond(bool *);
static void inquire(bool,bool *);
static void get_info(sg4::Mailbox * ,Source *,bool, int*,bool *);
static void worker();
static void work_distributor(double,bool,string);
static void monitor(sg4::Host *,bool *);
static void wait_for_inquiry(bool *,Result * );

int main(int argc, char* argv[])
{
	sg_host_energy_plugin_init();
	sg4::Engine e(&argc, argv);
	e.load_platform(argv[1]);
	std::vector<simgrid::s4u::Link*> links = e.get_all_links();
	for(int i=1;i<links.size();i++){
		auto src=links[i]->get_property("src");
		auto dst=links[i]->get_property("dst");
		netzone[src].push_back(dst);

		//auto dst1=links[i]->get_property("src"); // using the link for direct connection in both directions
		//auto src1=links[i]->get_property("dst");
		//netzone[src1].push_back(dst1);
	}
	bool *inquiring=new bool;

	*inquiring=true;
	sg4::Host * host_0=sg4::Host::by_name("host_2");//define the source...
	sg4::ActorPtr actor=sg4::Actor::create("actor_0",host_0, inquire,true,inquiring);
    e.run();
    XBT_INFO("End of simulation.");
  return 0;
}

static void monitor(sg4::Host *host,bool *busy){
		int counter=0;
		while(true){
			if (host->get_available_speed()<=0.4)
				{counter++;}
			else
				{counter=0;}
		sg4::this_actor::sleep_for(1);
		if(counter>60){*busy=true;break;}
		}}

static void wait_for_inquiry(bool *inquiring,Result *result_packet){
	while(true){
		sg4::this_actor::sleep_for(1);
	if(*inquiring==false){
		XBT_INFO("End of re-inquiring at source %s\n",result_packet->source_name.data());
		break;
	}
		}
	if(result_packet->source_name!=sources[0])
		sg4::ActorPtr actor=simgrid::s4u::Actor::create("new_source",sg4::Host::by_name(result_packet->source_name),work_distributor,result_packet->residue,false,"");
	else
		sg4::ActorPtr actor=simgrid::s4u::Actor::create("new_source",sg4::Host::by_name(result_packet->source_name),work_distributor,result_packet->residue,true,"");
	result_packet->residue=0;
	//result_packet->completion=true;

}

static void receive_outcome(sg4::Mailbox *mail_b,Result * result_packet,bool root,int *i){
	auto * result=static_cast<Result *>(mail_b->get());
	XBT_INFO("Receiving from %s result",mail_b->get_cname());
	result_packet->participants+="," +result->participants;
	result_packet->result+=","+result->result;
	result_packet->residue+=result->residue;
	XBT_INFO("Residue at %s is %f\n",result_packet->source_name.data(),result_packet->residue);
	*i+=1;
	if(root==false && *i==computing_tree[result_packet->source_name].size()&&result_packet->residue==0){
		XBT_INFO("Work completed at node %s. send forth up\n",result_packet->source_name.data());
		sg4::Mailbox::by_name(result_packet->source_name)->put(result_packet, result_packet->size);
		}

	else if(*i==computing_tree[result_packet->source_name].size()&&result_packet->residue>0){
		bool *inquiring=new bool;
		*inquiring=true;
		XBT_INFO("Work did not complete at %s. Let's redistribute the residue",result_packet->source_name.data());
		sg4::ActorPtr actor=sg4::Actor::create("actor_0",sg4::Host::by_name(result_packet->source_name), inquire,true,inquiring);// true because we update part of the computing tree not all the computing tree
		sg4::Actor::create("departed",sg4::Host::by_name(result_packet->source_name), wait_for_inquiry,inquiring, result_packet);
		}


	if(root==true && *i==computing_tree[result_packet->source_name].size()){
		XBT_INFO("This is the source %s",result_packet->source_name.data());
		XBT_INFO("Total participant hosts are:%s",result_packet->participants.data());
		XBT_INFO("Result is :\n %s",result_packet->result.data());
		XBT_INFO("Total Residue is:%f Mflops",result_packet->residue/1e6);
	}
}

static void local_worker(double work,Result* result_packet){
	bool * busy=new bool;
	*busy=false;
	double start=sg4::Engine::get_clock();
	//sg4::this_actor::execute(*work);//execution without monitoring the cpu power;
	double new_work;
	sg4::ActorPtr supervisor=sg4::Actor::create("monitor", sg4::this_actor::get_host(), monitor,sg4::this_actor::get_host(),busy);

	while(true){
	new_work=sg4::this_actor::get_host()->get_available_speed()*sg4::this_actor::get_host()->get_speed()*60;
	sg4::this_actor::execute(new_work);
	work=work-new_work;
	if(work<new_work){sg4::this_actor::execute(work);supervisor->kill(); result_packet->completion=true;break;}
	if (*busy==true){
		XBT_INFO("Host  %s was suspended at speed %f",sg4::this_actor::get_host()->get_cname(),sg4::this_actor::get_host()->get_available_speed());
		supervisor->kill();
		result_packet->completion=false;
		result_packet->residue=work;
		break;
		}
	}

	result_packet->execution_time=sg4::Engine::get_clock()-start;
	XBT_INFO("Work is done in %s; Duration: %f Seconds",sg4::this_actor::get_host()->get_cname(),(sg4::Engine::get_clock()-start));
	result_packet->result=sg4::this_actor::get_host()->get_name()+ " completed work in "+to_string(sg4::Engine::get_clock()-start)+" Sec\n";
	result_packet->participants=sg4::this_actor::get_host()->get_cname();
}

static void worker(){
		Result result_packet;
		bool * busy=new bool;
		*busy=false;
		sg4::Mailbox *mail_b=sg4::Mailbox::by_name(sg4::this_actor::get_host()->get_cname());
		auto *work=	static_cast<double *>(mail_b->get());
		double new_work;
		if(netzone[sg4::this_actor::get_host()->get_cname()].empty()==true){
			double start=sg4::Engine::get_clock();
			//sg4::this_actor::execute(*work);//execution without monitoring the cpu power;
			sg4::ActorPtr supervisor=sg4::Actor::create("monitor", sg4::this_actor::get_host(), monitor,sg4::this_actor::get_host(),busy);
			while(true){
			new_work=sg4::this_actor::get_host()->get_available_speed()*sg4::this_actor::get_host()->get_speed()*60;
			sg4::this_actor::execute(new_work);
			*work=*work-new_work;
			if(*work<new_work){sg4::this_actor::execute(*work);supervisor->kill();result_packet.completion=true;result_packet.residue=0;break;}
			if (*busy==true){// the condition should monitor the cpu power for long time
				XBT_INFO("Host %s was suspended at speed %f",sg4::this_actor::get_host()->get_cname(),sg4::this_actor::get_host()->get_available_speed());
				supervisor->kill();
				result_packet.completion=false;
				result_packet.residue=*work;
				break;
				}
			}
			result_packet.execution_time=sg4::Engine::get_clock()-start;
			XBT_INFO("Work is done in %s; Duration: %f Seconds",sg4::this_actor::get_host()->get_cname(),(sg4::Engine::get_clock()-start));
			result_packet.result=sg4::this_actor::get_host()->get_name()+ " completed work in "+to_string(sg4::Engine::get_clock()-start)+" Sec\n";
			result_packet.source_name=sg4::this_actor::get_host()->get_cname();
			result_packet.participants=sg4::this_actor::get_host()->get_cname();
			result_packet.size=1024;
			sg4::Mailbox::by_name(sg4::this_actor::get_host()->get_cname())->put(new Result( result_packet), result_packet.size);
		}
		else{
			simgrid::s4u::Actor::create("new_source",simgrid::s4u::Host::by_name(sg4::this_actor::get_host()->get_cname()),work_distributor,*work,false,"");
			}
			}

static void work_distributor(double work,bool root,string candinate_name){
	Result * result_packet=new Result();
	int *i=new int();
	std::string source_name=sg4::this_actor::get_host()->get_cname();
	result_packet->source_name=source_name;
	result_packet->residue=0;
	simgrid::s4u::Host *source=simgrid::s4u::Host::by_name(source_name);
	std::vector<double>computing_power;
	std::vector<double>RAM_size;

	//sg4::ActorPtr supervisor=sg4::Actor::create("monitor", sg4::this_actor::get_host(), monitor,sg4::this_actor::get_host(),busy);
	double total_RAM_size=0;
	double total_computing_power=0;
	if(source->get_available_speed()>0.4){
	 total_RAM_size=stod(source->get_property("RAM"));
	 total_computing_power=source->get_available_speed();}

	map<string,map<string,vector<double,double>>>::iterator it;

	if(candinate_name.empty()==true){
	for (auto host_name:computing_tree[source_name])
	{
		total_computing_power+=computing_tree[source_name][host_name.first][0];//{[0],[1]}===>{[computing_power],[ram size]}
		total_RAM_size+=computing_tree[source_name][host_name.first][1];
		computing_power.push_back(computing_tree[source_name][host_name.first][0]);
		RAM_size.push_back(computing_tree[source_name][host_name.first][1]);
	}
	double work_per_FLOP=work/total_computing_power;

	double work_per_host=0.0;
	if(source->get_available_speed()>0.4){

	sg4::Actor::create("local_worker",source, local_worker,(work_per_FLOP*source->get_available_speed()),result_packet);
	}
	std::vector<simgrid::s4u::Mailbox * >mail_box={};
	for(auto host:computing_tree[source_name]){			// 1)create new actors+ create new mail boxes named as new workers
		if(computing_tree[source_name][host.first][0]>0){
			work_per_host=work_per_FLOP*computing_tree[source_name][host.first][0];
			mail_box.push_back(simgrid::s4u::Mailbox::by_name(host.first));
			simgrid::s4u::Host *new_worker=simgrid::s4u::Host::by_name(host.first);
			sg4::Actor::create("new_worker",sg4::Host::by_name(host.first), worker);
			sg4::Mailbox::by_name(host.first)->put(new double(work_per_host), sizeof(work_per_host));
			XBT_INFO("Send %f Mflops from %s to %s",work_per_host/1e6,source_name.data(),new_worker->get_cname());
		}
		}
		for(auto mail_b:mail_box){
			sg4::Actor::create("receiver",source, receive_outcome,mail_b,result_packet,root,i);/////////
		}
	}
	else{
		sg4::Host *candinate=sg4::Host::by_name(candinate_name);
		total_computing_power=0;
		total_RAM_size=0;
		sg4::Mailbox *mail_box =sg4::Mailbox::by_name(candinate->get_name());
		total_computing_power=candinate->get_available_speed();
		total_RAM_size=stod(candinate->get_property("RAM"));
		sg4::Actor::create("new_worker",sg4::Host::by_name(candinate->get_name()), worker);
		mail_box->put(new double(work), sizeof(work));
		XBT_INFO("Send %f Mflops from %s to %s",work/1e6,source_name.data(),candinate->get_cname());
		sg4::Actor::create("receiver",sg4::Host::by_name(candinate->get_name()), receive_outcome,mail_box,result_packet,root,i);


	}

}

static void get_info(sg4::Mailbox * mail_b,Source* the_source,bool root, int *i,bool*inquiring){

	auto *host_specification = static_cast<Source *>(mail_b->get());// get from put in respond() function
	XBT_INFO("Receiving responses from %s...",mail_b->get_cname());
	XBT_INFO("Receiving from %s: %f of RAM memory",simgrid::s4u::this_actor::get_host()->get_cname(),host_specification->RAM);
	XBT_INFO("Receiving from %s: %f of computing power\n",simgrid::s4u::this_actor::get_host()->get_cname(),host_specification->computing_power);
	if(host_specification->computing_power>0.4){
		the_source->RAM+=host_specification->RAM;
		the_source->computing_power+=host_specification->computing_power;
		the_source->infected+=host_specification->source_name+" , "+host_specification->infected;
		the_source->size+=host_specification->size;
		computing_tree[the_source->source_name][host_specification->source_name]={host_specification->computing_power,host_specification->RAM};
	}
	else {computing_tree[the_source->source_name].erase(host_specification->source_name);}//////////

	*i+=1;
	if(root==false && *i==netzone[the_source->source_name].size()){
		sg4::Mailbox::by_name(the_source->source_name)->put(the_source,the_source->size);
	}

if(root==true && *i==netzone[the_source->source_name].size()){
	XBT_INFO("This is the source %s",the_source->source_name.data());
	XBT_INFO("RAM' total size is:  %f",the_source->RAM);
	XBT_INFO("CPUs' total power is:  %f",the_source->computing_power);
	XBT_INFO("Total infected hosts are:%s\n",the_source->infected.data());
	*inquiring=false;

	if(one_time_pad_work==true){
		sg4::Host * host_0=sg4::Host::by_name("host_2");
		double work_to_do=10E12;
		sg4::Actor::create("actor_0",host_0, work_distributor,work_to_do,true,"");

		one_time_pad_work=false;
	}

}
	}


static void inquire(bool root,bool *inquiring)
{
	int *i=new int();
	XBT_INFO("The source now is: %s ",sg4::this_actor::get_host()->get_cname());
	Source * the_source=new Source();
	string host_name=simgrid::s4u::this_actor::get_host()->get_cname();
	the_source->source_name=host_name;
	simgrid::s4u::Host *host=simgrid::s4u::Host::by_name(host_name);
	if(host->get_available_speed()>0.4){
		the_source->RAM=stod(host->get_property("RAM"));
		the_source->computing_power=host->get_available_speed();}

	sources.push_back(the_source->source_name);

	string message="send_info";
	std::vector<simgrid::s4u::Mailbox * >mail_box={};

	for(auto host:netzone[host_name]){					// 1)create new actors+ create new mail boxes named as new actors
	mail_box.push_back(simgrid::s4u::Mailbox::by_name(host));
	simgrid::s4u::Host *new_host=simgrid::s4u::Host::by_name(host);
	simgrid::s4u::Actor::create("new_actor", new_host, host_respond,inquiring);
}

for(auto mail_b:mail_box){
	mail_b->put(new std::string(message), message.size());                  // 2) send the request from the source node
	//XBT_INFO("Request has been sent from: %s!.\n",sg4::this_actor::get_host()->get_cname());

}

for(auto mail_b:mail_box){ 				// 3)receive the response from leaf nodes

		sg4::Actor::create("listener",sg4::Host::by_name(mail_b->get_cname()) , get_info,mail_b,the_source,root,i,inquiring);
}


}

static void host_respond(bool *inquiring)
{
	 auto *msg = static_cast<std::string*>(sg4::Mailbox::by_name(sg4::this_actor::get_host()->get_cname())->get());
	 string request=msg->c_str();
	 if(request=="send_info"){
	// XBT_INFO("Request has been received to: %s\n",sg4::this_actor::get_host()->get_cname());
	 Source host={};
	 if(netzone[sg4::this_actor::get_host()->get_cname()].empty()==true){


		 host.RAM=stod(sg4::this_actor::get_host()->get_property("RAM"));
		 host.computing_power=sg4::this_actor::get_host()->get_available_speed();
		 host.source_name=sg4::this_actor::get_host()->get_cname();
		 host.size=1024;
		 sg4::Mailbox::by_name(sg4::this_actor::get_host()->get_cname())->put(new Source(host), host.size);//send from leaf node;


	 }
	else{

		simgrid::s4u::Actor::create("new_source",simgrid::s4u::Host::by_name(sg4::this_actor::get_host()->get_cname()),inquire,false,inquiring);}

	 }
	 }
