#include "upnpnat.h"

int main (int argc,char * argv[]){

	UPNPNAT nat;
	nat.init(5,10);

	if(!nat.discovery()){
		printf("discovery error is %s\n",nat.get_last_error());
		return -1;
	}


	if(!nat.add_port_mapping("test","192.168.12.117",1234,1234,"TCP")){
		printf("add_port_mapping error is %s\n",nat.get_last_error());
		return -1;
	}

	printf("add port mapping succ.\n");

	return 0;
}
