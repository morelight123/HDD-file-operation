////////////////////////////////////////////////////////////////////////////////
//
//  File          : hdd_client.c
//  Description   : This is the client side of the CRUD communication protocol.
//
//   Author       : Patrick McDaniel
//  Last Modified : Thu Oct 30 06:59:59 EDT 2014
//

// Include Files
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>

// Project Include Files
#include <hdd_network.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>
#include <hdd_driver.h>



//global variable
int socket_fd=-1; 
////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_client_operationif (read(socket_fd, &value, sizeof(value))!= sizeof(value) ) {
// Description  : This the client operation that sends a request to the CRUD
//                server.   It will:
//
//                1) if INIT make a connection to the server
//                2) send any request to the server, returning results
//                3) if CLOSE, will close the connection
//
// Inputs       : cmd - the request opcode for the command
//                buf - the block to be read/written from (READ/WRITE)
// Outputs      : the response structure encoded as needed

HddBitResp hdd_client_operation(HddBitCmd cmd, void *buf) {
printf("op: %d; bs: %d;  Flags: %d, R: %d,  bID: %d\n",cmd>>62,(cmd>>36) & 0x3FFFFFF, (cmd>>33) & 0x7, (cmd>>32) &0x1, cmd& 0xFFFFFFFF);
	int cmdLength = HDD_NET_HEADER_SIZE;
	struct sockaddr_in caddr; 
	int op=(cmd>>62);
	int Flags=(cmd>>33) & 0x7;
	HddBitResp *resp=malloc(sizeof(HddBitCmd));
	HddBitCmd *value=malloc(sizeof(HddBitCmd));
	*value=htonll64(cmd);  
	int blockSize=(cmd>>36) & 0x3FFFFFF;
	if(Flags==(HDD_INIT)) { //process for HDD initial
		printf("Flags is HDD_INIT\n"); 
		caddr.sin_family = AF_INET; 
		caddr.sin_port = htons(HDD_DEFAULT_PORT);
 		socket_fd = socket(PF_INET, SOCK_STREAM, 0);
		printf("socket: %d\n",socket_fd);
		if (socket_fd==-1) { //check socket_fd value
			printf( "Error on socket creation [%s]\n", strerror(errno) );
			return( -1 );     
		}  
		if ( inet_aton(HDD_DEFAULT_IP, &caddr.sin_addr) == 0 ) { 
			printf( "Error on inet_aton\n");
			return( -1 ); 
		}
		printf("complete inet_aton\n");
		//check connection
		if ( connect(socket_fd, (const struct sockaddr *)&caddr, sizeof(struct sockaddr)) == -1 ) {
			printf( "Error on socket connect [%s]\n", strerror(errno) );
			return( -1 );
		}
		printf("start connecting\n");//bug checking
	}
printf("send cmd up\n");
	//send cmd upto drive
	if (write(socket_fd, value, cmdLength)!= cmdLength) {
		printf( "Error write cmd up[%s]\n",strerror(errno) ); 
		return( -1 );
	}
	printf("end sending cmd up\n");
	//check if need to write buf into drive
	if(op == HDD_BLOCK_CREATE || op == HDD_BLOCK_OVERWRITE){
		printf("op is HDD_BLOCK_CREATE or HDD_BLOCK_OVERWRITE\n");
		if (write(socket_fd, buf, blockSize) != blockSize) {
			printf( "Error writing network data in HDD_BLOCK_CREATE or HDD_BLOCK_OVERWRITE[%s]\n", strerror(errno));
		return( -1 );
		}
	}

	//read the cmd back
    	printf("start receive resp socket: %d\n",socket_fd);//bug checking
	if (read(socket_fd, resp, cmdLength)!= cmdLength){
		printf( "Error reading network data. op=HDD_BLOCK_READ [%s]\n", strerror(errno) ); 
		return( -1 );
	}
	printf("end receive resp\n"); //bug checking
	HddBitResp returnValue;
	int readSize;
	returnValue=ntohll64(*resp);
	blockSize=(returnValue>>36) & 0x3FFFFFF;
	if(op==HDD_BLOCK_READ) { //get read stuff into buf
		printf("op is HDD_BLOCK_READ\n");
		readSize=read(socket_fd, buf, blockSize);
		while(1) {
			if(readSize>=blockSize)
				break;
			readSize=readSize+read(socket_fd, &buf[readSize], blockSize - readSize);
		}
	}
	if(Flags==HDD_SAVE_AND_CLOSE) { //check for HDD close
		printf("Flags is HDD_SAVE_AND_CLOSE\n");
		close( socket_fd );
		socket_fd = -1;
	}

//free allocated space

	free(resp);
	free(value);
	return returnValue; // Not a vvalid return value
}







