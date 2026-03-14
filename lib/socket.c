#include "socket.h"

int createTCPIpv4Socket ()
{	return socket(AF_INET,SOCK_STREAM,0);	}

// for a server program the ip field is left empty because the server doesn't care which devices connect
// a client program needs to pass the specific IP of the server to which it wants to connect

struct sockaddr* createTCPIpv4SocketAddress (char* ip, int port_num)
{
	// dynamic initialization of address
		
	struct sockaddr_in* address = (struct sockaddr_in*) malloc (sizeof(struct sockaddr_in));

	// htons() function corrects the endianness of the port_num's binary value according to the architecture

	address->sin_port = htons(port_num);

	// specifying address family

	address->sin_family = AF_INET;

	// distinction between server and client program using the function
	// inet_pton() function converts IP from string to binary form

	if (strlen(ip) == 0) 
		address->sin_addr.s_addr = INADDR_ANY;
	else
		inet_pton(AF_INET,ip,&address->sin_addr.s_addr); 

	// most functions need the address with the data type sock_addr
	// hence we cast the data type for IPv4 socket addresses (sock_addr_in) into sock_addr 

	return (struct sockaddr*) address;
}
