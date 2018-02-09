////////////////////////////////////////////////////////////////////////////////
//
//  File           : hdd_file_io.c
//  Description    : 
//
//  Author         : Kaixuan Meng
//  Last Modified  : 
//

// Includes
#include <malloc.h>
#include <string.h>

// Project Includes
#include <hdd_file_io.h>
#include <hdd_driver.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>
#include <hdd_network.h>

// Defines
#define CIO_UNIT_TEST_MAX_WRITE_SIZE 1024
#define HDD_IO_UNIT_TEST_ITERATIONS 10240


// Type for UNIT test interface
typedef enum {
	CIO_UNIT_TEST_READ   = 0,
	CIO_UNIT_TEST_WRITE  = 1,
	CIO_UNIT_TEST_APPEND = 2,
	CIO_UNIT_TEST_SEEK   = 3,
} HDD_UNIT_TEST_TYPE;

char *cio_utest_buffer = NULL;  // Unit test buffer



//Implementation
//global struct for files 
struct _fileInfo {
	char path[MAX_FILENAME_LENGTH];
	int16_t FileHandle;
	uint32_t blockID;
	uint32_t currPos;
	uint32_t blockSize;
};
struct _openedFile {
	struct _fileInfo file[MAX_HDD_FILEDESCR];
	int Initialized;
};
struct _openedFile openedFile;

//helper function, pre set global variables
void defineMetaData() {
	printf("Enter defineMetaData\n");
	for(int q=0; q<MAX_HDD_FILEDESCR;++q) {
		memset(openedFile.file[q].path,'\0',sizeof(openedFile.file[q].path));
		openedFile.file[q].FileHandle=-1;
		openedFile.file[q].blockID=0;
		openedFile.file[q].currPos=0;
		openedFile.file[q].blockSize=0;
	}
}

//helper function, build up command with op, blockSize, flags, R and blockID
HddBitCmd setCommand(int op,uint32_t blockSize, int Flags, int R, uint32_t blockID) {
	printf("Enter setCommand\nop: %d; bs: %d;  Flags: %d, R: %d,  bID: %d\n",op,blockSize, Flags, R, blockID);
	HddBitCmd command;
	memset(&command,0, sizeof(HddBitCmd));
	command=op<<26; //initialize command with op=2, which means wirite to the block
	command=(command | blockSize)<<3;
	command=(command | Flags)<<1;
	command=(command | R)<<32;
	command=command | (blockID);
	return command;
}
int getResult(HddBitResp resp, int *op,uint32_t *blockSize, int *Flags, int *R, uint32_t *blockID) {
	printf("Enter getResult\n");
	*op=resp>>62;
	*blockSize=(resp>>36) & 0x3FFFFFF;
	*Flags=(resp>>33) & 0x7;
	*R=(resp>>32) &0x1;
	*blockID=resp & 0xFFFFFFFF;
	return *R;
}
//helper function: locate requested file in global struct
int findFile(int16_t fh) {
	printf("Enter findFile\n");
	int i=0;
	while(openedFile.file[i].FileHandle!=-1) {
		if(openedFile.file[i].FileHandle==fh) {//find the right file
			break;
		}
		++i;
	}
	if(openedFile.file[i].FileHandle==-1){//file with fh is not in the list
		printf("file with fh: %d is not opened\n",fh);
		return -1;
	}
printf("i: %d   fh: %d, openedFile.file[i].FileHandle: %d, blockID: %d\n",i,fh,openedFile.file[i].FileHandle,openedFile.file[i].blockID);
	return i;
}

//helper function: check for initization
int initCheck() {
	printf("Enter initCheck\n");
	if (openedFile.Initialized!=0) {
		return 0;
	}
	HddBitCmd cmd=0;
	int op=0;
	uint32_t blockSize=0;
	int Flags=0;
	int R=0;
	uint32_t blockID=0;
	cmd=setCommand(HDD_DEVICE,0, HDD_INIT, 0, 0);
	HddBitResp resp=hdd_client_operation(cmd, NULL);
	getResult(resp, &op,&blockSize, &Flags, &R, &blockID);
	if (openedFile.Initialized==0) {
		if (R==-1) { //check for initialization
			printf("Initialization fail for the device;R= %d\n",R);
			return -1;}
	    	openedFile.Initialized=1;
		defineMetaData();
		return 0;}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_format
// Description  : Initializes the device if not already done, Sends a format request to the device which will delete all blocks
//
// Inputs       : None
// Outputs      : 0 on success, -1 on fail
//
uint16_t hdd_format(void) {
	printf("Enter hdd_format\n");
	char *buffer=malloc(HDD_MAX_BLOCK_SIZE);//define a buffer for later use
	if(initCheck()==-1)
		return -1;
	HddBitCmd command=0;
	int op=0;
	uint32_t blockSize=0;
	int Flags=HDD_FORMAT;
	int R=0;
	uint32_t blockID=0; //set it to meta block
	command=setCommand(HDD_DEVICE,blockSize,Flags,R,blockID);
	HddBitResp resp=hdd_client_operation(command,NULL);//Sends a format request to the device which will delete all blocks 
	if(getResult(resp, &op,&blockSize, &Flags, &R, &blockID)==-1) {
		return -1;
	}
	blockID=0;
	command=setCommand(HDD_BLOCK_CREATE,HDD_MAX_BLOCK_SIZE, HDD_META_BLOCK, 0, blockID);//Set command for create meta block
	memcpy(buffer, &openedFile,sizeof(openedFile));//copy global struct information into buffer for writing to the block
	resp=hdd_client_operation(command,buffer);//create meta block and save global data structure to it
	free(buffer);
	return 0;
}



////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_mount
// Description  : Initializes the device if not already done, Reads from the meta block to populate your global data structure with previously saved values
//
// Inputs       : None
// Outputs      : 0 on success, -1 on fail
//
uint16_t hdd_mount(void) {
	printf("Enter hdd_mount\n");
	if(initCheck()==-1)//check for initization
		return -1;
	HddBitCmd command=0;

	int op=0;
	int Flags=0;
	int R=0;

	char *buffer=malloc(HDD_MAX_BLOCK_SIZE);//define buffer for read data
	uint32_t blockID=0; //set it to meta block
	uint32_t blockSize=HDD_MAX_BLOCK_SIZE;
printf("before mount set command\n");
	command=setCommand(HDD_BLOCK_READ,blockSize, HDD_META_BLOCK, 0x0, blockID);
printf("after mount set command\n");
	HddBitResp blockInfo=hdd_client_operation(command,buffer);//read meta block data into buffer
	if(getResult(blockInfo, &op,&blockSize, &Flags, &R, &blockID)==-1) {
		return -1;
	}
	memcpy(&openedFile, buffer, sizeof(openedFile));// copy files info back to global struct
	free(buffer);
	return 0;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_unmount
// Description  : Saves the current state of the global data structure to the meta block, Sends a save and close request to the device, which will create/update the hdd_content.svd file
//
// Inputs       : None
// Outputs      : 0 on success, -1 on fail
//
uint16_t hdd_unmount(void) {
	printf("Enter hdd_unmount\n");
	char *buffer=malloc(HDD_MAX_BLOCK_SIZE);
	HddBitCmd command=0;
	int op=0;
	int Flags=0;
	int R=0;
	uint32_t blockID=0; //set it to meta block
	uint32_t blockSize=HDD_MAX_BLOCK_SIZE;//get block size
	command=setCommand(HDD_BLOCK_OVERWRITE,blockSize, HDD_META_BLOCK, 0, blockID);
	memcpy(buffer, &openedFile,sizeof(openedFile));//copy file info to buffer
printf("before	blockInfo=hdd_client_operation(command,data)\n");
	HddBitResp blockInfo=hdd_client_operation(command,buffer);//save the current state of global data structure to the meta block
	if(getResult(blockInfo, &op,&blockSize, &Flags, &R, &blockID)==-1) {
		printf("hdd_unmount error\n");
		return -1;
	}
	command=setCommand(HDD_DEVICE,0x0,HDD_SAVE_AND_CLOSE,0,0x0);
printf("before	hdd_client_operation(command,NULL);\n");
	hdd_client_operation(command,NULL);//Sends a save and close request to the device, which will create/update the hdd_content.svd file
	if(getResult(blockInfo, &op,&blockSize, &Flags, &R, &blockID)==-1) {
		return -1;
	}
	free(buffer);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_open
// Description  : open a file, set metadata in global data structure. return a unique integer file handle.
//
// Inputs       : char *path
// Outputs      : unique integer file handle
//
int16_t hdd_open(char *path) {
	printf("Enter hdd_open\n");
	int i=0;
	if(initCheck()==-1)//device initization check
		return -1;
	while(openedFile.file[i].FileHandle!=-1) {//find next available spot for requested file
		if(strcmp(openedFile.file[i].path,path)==0) {
			if(openedFile.file[i].FileHandle==-2) {//if file is deleted
				openedFile.file[i].FileHandle=i;//recover file
				return i;}
			printf("file is already opened.\n");
			return openedFile.file[i].FileHandle;
		}
		++i;
	}
	memset(&openedFile.file[i].blockID, 0, sizeof(int16_t));//pre allocate space
	if (strcmp(path,"")==0) { //path is NULL so no file is openning
		return -1;}
	strcpy(openedFile.file[i].path,path);//save meta info
	openedFile.file[i].FileHandle=i;
	return openedFile.file[i].FileHandle;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_close
// Description  : close the file that filehandle point to. delete all content.
//
// Inputs       : fh
// Outputs      : 0 success, -1 failure
//
int16_t hdd_close(int16_t fh) {
	printf("Enter hdd_close\n");
	int i=0;
	while(openedFile.file[i].FileHandle!=-1) {//find the file in meta info
		if(openedFile.file[i].FileHandle==fh) {
			break;
		}
		++i;
	}
	if(openedFile.file[i].FileHandle==-1){//file with fh is not in the list
		printf("file with fh: %d is not opened\n",fh);
		return -1;
	}
	//hdd_delete_block(file.blockID);//delete block
	openedFile.file[i].FileHandle=-2;//reset every metadata
	openedFile.file[i].currPos=0;//reset file pos
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_read
// Description  : read count bytes of data from specific block that file handle point to
//
// Inputs       : file handle, data, count
// Outputs      : number of bytes read
//
int32_t hdd_read(int16_t fh, void * data, int32_t count) {
	printf("Enter hdd_read\n");//check for bugs
	int op=0;
	int Flags=0;
	int R=0;
	uint32_t blockID=0;
	int i=findFile(fh);
	if(i==-1)//can't find file
		return -1;
	uint32_t blockSize=openedFile.file[i].blockSize;
	HddBitCmd command;
	memset(&command,0, sizeof(HddBitCmd));
	HddBitResp blockInfo;
	memset(&blockInfo,0, sizeof(HddBitResp));
	if(fh!=openedFile.file[i].FileHandle || openedFile.file[i].blockID==0) {//requested file is not opened
		printf("requested file is not opened\n");
		return -1;}
	if(openedFile.file[i].currPos+count>blockSize) {//request data is out of range of blocksize
		command=setCommand(0x1,blockSize, 0x0, 0x0, openedFile.file[i].blockID);
//initialize command with op=2, which means wirite to the block
		char *readData=(char*)malloc(blockSize);
		blockInfo=hdd_client_operation(command,readData);
		if(getResult(blockInfo, &op,&blockSize, &Flags, &R, &blockID)==-1) {
			printf("read error 1 in hdd_read\n");		
			return -1;
	}
		memcpy(data,&readData[openedFile.file[i].currPos], blockSize-openedFile.file[i].currPos);
		uint32_t temp=blockSize-openedFile.file[i].currPos;//address number of bytes that is read
		openedFile.file[i].currPos=blockSize;
		free(readData);
		return temp;}//required reading content is out of range
	command=setCommand(0x1,blockSize, 0x0, 0x0, openedFile.file[i].blockID); //initialize command with op=1, which means read the block
	char *readData=(char*)malloc(blockSize);
	blockInfo=hdd_client_operation(command,readData);
	if(getResult(blockInfo, &op,&blockSize, &Flags, &R, &blockID)==-1) {
			printf("read error 2 in hdd_read\n");		
			return -1;
	}
	memcpy(data,&readData[openedFile.file[i].currPos], count);
	openedFile.file[i].currPos=openedFile.file[i].currPos+count;
	R=blockInfo >>32& 0x01;
	if(R==1){
		printf("R is 1\n");
		return -1;}
	free(readData);
	return count;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_write
// Description  : write data to the specific block
//
// Inputs       : fileHandle, data, count
// Outputs      : number of bytes that is written to the block
//
int32_t hdd_write(int16_t fh, void *data, int32_t count) {
	printf("enter write\n");//bug checking
	int op=0;
	int Flags=0;
	int R=0;
	uint32_t blockID=0;
	uint32_t blockSize=0;
	HddBitCmd command;
	memset(&command,0, sizeof(HddBitCmd));
	HddBitResp blockInfo;
	memset(&blockInfo,0, sizeof(HddBitResp));
	int i=findFile(fh);
	if(i<0) {//can't find file
		return -1;}
	if(openedFile.file[i].blockID==0) {//no block is created. create a block the first time

		command=setCommand(0x0,count, 0x0, 0x0, 0x0);//set up command
		blockInfo=hdd_client_operation(command,data);//create new block
		if(getResult(blockInfo, &op,&blockSize, &Flags, &R, &blockID)==-1) {
			printf("write error 1 in hdd_write\n");		
			return -1;
		}
		openedFile.file[i].blockID=blockID;
		openedFile.file[i].currPos=count;
		openedFile.file[i].blockSize=count;
		return count;
	}
	else {//block was created and overwrite need to be implemented
		uint32_t blockSize=openedFile.file[i].blockSize;
		if(openedFile.file[i].currPos+count<=blockSize){ //overwrite, need not to create new block
			printf("In write: Enter if\n");//bug checking
			char *readData=(char*)malloc(blockSize);
			command=setCommand(HDD_BLOCK_READ,blockSize, 0x0, 0x0, openedFile.file[i].blockID);
			blockInfo=hdd_client_operation(command,readData);//read entire content from block
			command=0;
			command=setCommand(HDD_BLOCK_OVERWRITE,blockSize, 0x0, 0x0, openedFile.file[i].blockID);
			memcpy(&readData[openedFile.file[i].currPos], data, count);
			blockInfo=hdd_client_operation(command,readData);//overwrite the block with new content
			if(getResult(blockInfo, &op,&blockSize, &Flags, &R, &blockID)==-1) {
				printf("write error 2 in hdd_write\n");		
				return -1;}
			openedFile.file[i].currPos=openedFile.file[i].currPos+count;
			free(readData);
			return count;}
		else { //need to create new block because data is larger than original block size
printf("In write: Enter else\n");
			char *readData=(char*)malloc(openedFile.file[i].currPos+count);
			command=setCommand(HDD_BLOCK_READ,blockSize, 0x0, 0x0, openedFile.file[i].blockID);//set up command
			blockInfo=hdd_client_operation(command,readData);//read entire data from block first
			if(getResult(blockInfo, &op,&blockSize, &Flags, &R, &blockID)==-1) {
				printf("write error 3 in hdd_write\n");		
				return -1;}
			command=0;
			command=setCommand(HDD_BLOCK_CREATE,openedFile.file[i].currPos+count, 0x0, 0x0, 0x0);
			memcpy(&readData[openedFile.file[i].currPos], data, count);//overwrite content
			blockInfo=hdd_client_operation(command,readData);//create new block with new content.
			if(getResult(blockInfo, &op,&blockSize, &Flags, &R, &blockID)==-1) {
				printf("write error 3 in hdd_write\n");		
				return -1;}
			command=setCommand(HDD_BLOCK_DELETE,0x0, 0x0, 0x0, openedFile.file[i].blockID);//delete old block
			openedFile.file[i].blockID=blockID;//get the blockID
			openedFile.file[i].blockSize=openedFile.file[i].currPos+count;//update blockSize
			openedFile.file[i].currPos=openedFile.file[i].currPos+count;//address file pointer
			free(readData);
			return count;}
	}
	return -1;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_seek
// Description  : move position(file pointer) in file.
//
// Inputs       : fh, loc(requested position)
// Outputs      : 0 success, -1 failure
//
int32_t hdd_seek(int16_t fh, uint32_t loc) {
	printf("enter seek\n");//bug checking
	int i=findFile(fh);
	if(i<0) {
		return -1;}
	if(loc>openedFile.file[i].blockSize)
		return -1;
	openedFile.file[i].currPos=loc;  //assign file pointer to position loc
	printf("currPos: %d\n",openedFile.file[i].currPos);//bug checking
	return 0; //success
}








////////////////////////////////////////////////////////////////////////////////
//
// Function     : hddIOUnitTest
// Description  : Perform a test of the HDD IO implementation
//
// Inputs       : None
// Outputs      : 0 if successful or -1 if failure

int hddIOUnitTest(void) {

	// Local variables
	uint8_t ch;
	int16_t fh, i;
	int32_t cio_utest_length, cio_utest_position, count, bytes, expected;
	char *cio_utest_buffer, *tbuf;
	HDD_UNIT_TEST_TYPE cmd;
	char lstr[1024];

	// Setup some operating buffers, zero out the mirrored file contents
	cio_utest_buffer = malloc(HDD_MAX_BLOCK_SIZE);
	tbuf = malloc(HDD_MAX_BLOCK_SIZE);
	memset(cio_utest_buffer, 0x0, HDD_MAX_BLOCK_SIZE);
	cio_utest_length = 0;
	cio_utest_position = 0;

	// Format and mount the file system
	if (hdd_format() || hdd_mount()) {
		logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : Failure on format or mount operation.");
		return(-1);
	}

	// Start by opening a file
	fh = hdd_open("temp_file.txt");
	if (fh == -1) {
		logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : Failure open operation.");
		return(-1);
	}

	// Now do a bunch of operations
	for (i=0; i<HDD_IO_UNIT_TEST_ITERATIONS; i++) {

		// Pick a random command
		if (cio_utest_length == 0) {
			cmd = CIO_UNIT_TEST_WRITE;
		} else {
			cmd = getRandomValue(CIO_UNIT_TEST_READ, CIO_UNIT_TEST_SEEK);
		}
		logMessage(LOG_INFO_LEVEL, "----------");

		// Execute the command
		switch (cmd) {

		case CIO_UNIT_TEST_READ: // read a random set of data
			count = getRandomValue(0, cio_utest_length);
			logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : read %d at position %d", count, cio_utest_position);
			bytes = hdd_read(fh, tbuf, count);
			if (bytes == -1) {
				logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : Read failure.");
				return(-1);
			}

			// Compare to what we expected
			if (cio_utest_position+count > cio_utest_length) {
				expected = cio_utest_length-cio_utest_position;
			} else {
				expected = count;
			}
			if (bytes != expected) {
				logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : short/long read of [%d!=%d]", bytes, expected);
				return(-1);
			}
			if ( (bytes > 0) && (memcmp(&cio_utest_buffer[cio_utest_position], tbuf, bytes)) ) {

				bufToString((unsigned char *)tbuf, bytes, (unsigned char *)lstr, 1024 );
				logMessage(LOG_INFO_LEVEL, "CIO_UTEST R: %s", lstr);
				bufToString((unsigned char *)&cio_utest_buffer[cio_utest_position], bytes, (unsigned char *)lstr, 1024 );
				logMessage(LOG_INFO_LEVEL, "CIO_UTEST U: %s", lstr);

				logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : read data mismatch (%d)", bytes);
				return(-1);
			}
			logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : read %d match", bytes);


			// update the position pointer
			cio_utest_position += bytes;
			break;

		case CIO_UNIT_TEST_APPEND: // Append data onto the end of the file
			// Create random block, check to make sure that the write is not too large
			ch = getRandomValue(0, 0xff);
			count =  getRandomValue(1, CIO_UNIT_TEST_MAX_WRITE_SIZE);
			if (cio_utest_length+count >= HDD_MAX_BLOCK_SIZE) {

				// Log, seek to end of file, create random value
				logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : append of %d bytes [%x]", count, ch);
				logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : seek to position %d", cio_utest_length);
				if (hdd_seek(fh, cio_utest_length)) {
					logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : seek failed [%d].", cio_utest_length);
					return(-1);
				}
				cio_utest_position = cio_utest_length;
				memset(&cio_utest_buffer[cio_utest_position], ch, count);

				// Now write
				bytes = hdd_write(fh, &cio_utest_buffer[cio_utest_position], count);
				if (bytes != count) {
					logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : append failed [%d].", count);
					return(-1);
				}
				cio_utest_length = cio_utest_position += bytes;
			}
			break;

		case CIO_UNIT_TEST_WRITE: // Write random block to the file
			ch = getRandomValue(0, 0xff);
			count =  getRandomValue(1, CIO_UNIT_TEST_MAX_WRITE_SIZE);
			// Check to make sure that the write is not too large
			if (cio_utest_length+count < HDD_MAX_BLOCK_SIZE) {
				// Log the write, perform it
				logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : write of %d bytes [%x]", count, ch);
				memset(&cio_utest_buffer[cio_utest_position], ch, count);
				bytes = hdd_write(fh, &cio_utest_buffer[cio_utest_position], count);
				if (bytes!=count) {
					logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : write failed [%d].", count);
					return(-1);
				}
				cio_utest_position += bytes;
				if (cio_utest_position > cio_utest_length) {
					cio_utest_length = cio_utest_position;
				}
			}
			break;

		case CIO_UNIT_TEST_SEEK:
			count = getRandomValue(0, cio_utest_length);
			logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : seek to position %d", count);
			if (hdd_seek(fh, count)) {
				logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : seek failed [%d].", count);
				return(-1);
			}
			cio_utest_position = count;
			break;

		default: // This should never happen
			CMPSC_ASSERT0(0, "HDD_IO_UNIT_TEST : illegal test command.");
			break;

		}

	}

	// Close the files and cleanup buffers, assert on failure
	if (hdd_close(fh)) {
		logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : Failure close close.", fh);
		return(-1);
	}
	free(cio_utest_buffer);
	free(tbuf);

	// Format and mount the file system
	if (hdd_unmount()) {
		logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : Failure on unmount operation.");
		return(-1);
	}

	// Return successfully
	return(0);
}
