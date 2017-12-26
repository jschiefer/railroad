/*
 *  Created by Maximilian Goldschmidt <maxigoldschmidt@gmail.com>
 *  If you need Help, feel free to contact me any time!
 *  Do with this whatever you want, but keep thes Header and tell
 *  the others what you changed!
 *
 *  Last changed: 2017-02-13
 * 
 *  Joerg Dorchain <joerg@dorchain.net> 26.12.2017
 *  Ported to C and linux socketcan
 */


#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "mcan.h"

#define BIT(i) (1<<i)
#define bitWrite(a, b, c) ((c)?(a|=BIT(b)):(a&=~BIT(b)))
#define bitRead(a, b) ((a&BIT(b))?1:0)

bool mcan_debug = 0;

uint16_t generateHash(uint32_t uid){

	uint16_t highbyte = uid >> 16;
	uint16_t lowbyte = uid;
	uint16_t hash = highbyte ^ lowbyte;
	bitWrite(hash, 7, 0);
	bitWrite(hash, 8, 1);
	bitWrite(hash, 9, 1);

	return hash;
}

uint16_t generateLocId(uint16_t prot, uint16_t adrs){

	if(prot == 0) prot = DCC_ACC;
	if(prot == 1) prot = MM_ACC;

	return (prot + adrs - 1);
}

uint16_t getadrs(uint16_t prot, uint16_t locid){
  return (locid + 1 - prot);
}

void sendCanFrame(int canfd, MCANMSG can_frame){
/* XXX review */
	struct can_frame frm;
	
	memset (&frm, 0, sizeof(frm));

	uint32_t txId = can_frame.cmd;
	txId = (txId << 17) | can_frame.hash;
	bitWrite(txId, 16, can_frame.resp_bit);

	frm.can_id = htonl(txId);
	frm.can_dlc = can_frame.dlc;
	memcpy(&frm.data, can_frame.data, 8);
	
	write(canfd, &frm, sizeof (frm));

//	can.sendMsgBuf(txId, 1, can_frame.dlc, can_frame.data);
//	Serial.println(canFrameToString(can_frame, 1));
}

void sendDeviceInfo(int canfd, CanDevice device, int configNum){
	/*
	 *  Sendet die Basisinformationen über das Gerät an die GUI der CS2.
	 */
	MCANMSG can_frame;
	int frameCounter = 0;

	can_frame.cmd = CONFIG;
	can_frame.resp_bit = 1;
	can_frame.dlc = 8;
	for(int i = 0; i < 8; i++){can_frame.data[i] = 0;}
	can_frame.data[1] = configNum;
	can_frame.data[7] = device.boardNum;
	can_frame.hash = 0x0301;

	sendCanFrame(canfd, can_frame);
	frameCounter++;

	for(int i = 0; i < 8; i++) { can_frame.data[i] = 0; }
	strncpy(can_frame.data, device.artNum, 8);

	can_frame.hash++;
	sendCanFrame(canfd, can_frame);
	frameCounter++;

	int nameLen = strlen(device.name);
	int neededFrames;

	if(nameLen % 8) neededFrames = (nameLen / 8) + 1;
	else neededFrames = nameLen / 8;

	for(int i = 0; i < neededFrames; i++){
		for(int j = 0; j < 8; j++){
			if((8*i)+j < nameLen) can_frame.data[j] = device.name[(8*i)+j];
			else can_frame.data[j] = 0;
		}
		can_frame.hash++;
		sendCanFrame(canfd, can_frame);
		frameCounter++;
	}

	can_frame.hash = device.hash;
	can_frame.dlc = 6;
	can_frame.data[0] = device.uid >> 24;
	can_frame.data[1] = device.uid >> 16;
	can_frame.data[2] = device.uid >> 8;
	can_frame.data[3] = device.uid;
	can_frame.data[4] = 0;
	can_frame.data[5] = frameCounter;

	sendCanFrame(canfd, can_frame);
}

void sendConfigInfoDropdown(int canfd, CanDevice device, uint8_t configChanel, uint8_t numberOfOptions, uint8_t defaultSetting, char *settings){
	MCANMSG can_frame;
	int frameCounter;

	can_frame.cmd = CONFIG;
	can_frame.resp_bit = 1;
	can_frame.hash = 0x0301;
	can_frame.dlc = 8;

	//Erster Frame mit Grundinformationen:

	can_frame.data[0] = configChanel;
	can_frame.data[1] = 1;
	can_frame.data[2] = numberOfOptions;
	can_frame.data[3] = defaultSetting;
	for(int i = 4; i < 8; i++){
		can_frame.data[i] = 0;
	}

	sendCanFrame(canfd, can_frame);
	can_frame.hash++;
	frameCounter++;

	//Frames, die Strings enthalten:

	int length = strlen(settings);
	int neededFrames;

	if(length % 8) neededFrames = (length / 8) + 1;
	else neededFrames = length / 8;

	for(int i = 0; i < neededFrames; i++){
		for(int j = 0; j < 8; j++){
			if(((8*i)+j < length) && (settings[(8*i)+j] != '_')) can_frame.data[j] = settings[(8*i)+j];
			else can_frame.data[j] = 0;
		}

		sendCanFrame(canfd, can_frame);
		can_frame.hash++;
		frameCounter++;
	}

	//Abschließender bestätigungsframe:

	can_frame.hash = device.hash;
	can_frame.dlc = 6;
	can_frame.data[0] = device.uid >> 24;
	can_frame.data[1] = device.uid >> 16;
	can_frame.data[2] = device.uid >> 8;
	can_frame.data[3] = device.uid;
	can_frame.data[4] = configChanel;
	can_frame.data[5] = frameCounter;

	sendCanFrame(canfd, can_frame);
}

void sendConfigInfoSlider(int canfd, CanDevice device, uint8_t configChanel, uint16_t lowerValue, uint16_t upperValue, uint16_t defaultValue, char *settings){
	MCANMSG can_frame;
	int frameCounter;

	can_frame.cmd = CONFIG;
	can_frame.resp_bit = 1;
	can_frame.hash = 0x0301;
	can_frame.dlc = 8;

	//Erster Frame mit Grundinformationen:

	can_frame.data[0] = configChanel;
	can_frame.data[1] = 2;
	can_frame.data[2] = lowerValue >> 8;
	can_frame.data[3] = lowerValue;
	can_frame.data[4] = upperValue >> 8;
	can_frame.data[5] = upperValue;
	can_frame.data[6] = defaultValue >> 8;
	can_frame.data[7] = defaultValue;

	sendCanFrame(canfd, can_frame);
	can_frame.hash++;
	frameCounter++;

	//Frames, die Strings enthalten:

	int length = strlen(settings);
	int neededFrames;

	if(length % 8) neededFrames = (length / 8) + 1;
	else neededFrames = length / 8;

	for(int i = 0; i < neededFrames; i++){
		for(int j = 0; j < 8; j++){
			if(((8*i)+j < length) && (settings[(8*i)+j] != '_')) can_frame.data[j] = settings[(8*i)+j];
			else can_frame.data[j] = 0;
		}

		sendCanFrame(canfd, can_frame);
		can_frame.hash++;
		frameCounter++;
	}

	//Abschließender bestätigungsframe:

	can_frame.hash = device.hash;
	can_frame.dlc = 6;
	can_frame.data[0] = device.uid >> 24;
	can_frame.data[1] = device.uid >> 16;
	can_frame.data[2] = device.uid >> 8;
	can_frame.data[3] = device.uid;
	can_frame.data[4] = configChanel;
	can_frame.data[5] = frameCounter;

	sendCanFrame(canfd, can_frame);
}

void sendPingFrame(int canfd, CanDevice device, bool response){
	MCANMSG can_frame;
	can_frame.cmd = PING;
	can_frame.hash = device.hash;
	can_frame.resp_bit = response;
	can_frame.dlc = 8;
	can_frame.data[0] = device.uid >> 24;
	can_frame.data[1] = device.uid >> 16;
	can_frame.data[2] = device.uid >> 8;
	can_frame.data[3] = device.uid;
	can_frame.data[4] = device.versHigh;
	can_frame.data[5] = device.versLow;
	can_frame.data[6] = device.type >> 8;
	can_frame.data[7] = device.type;

	sendCanFrame(canfd, can_frame);
}

void switchAccResponse(int canfd, CanDevice device, uint32_t locId, bool state){

	MCANMSG can_frame;
	int adrs;

	can_frame.cmd = SWITCH_ACC;
	can_frame.resp_bit = 1;
	can_frame.hash = device.hash;
	can_frame.dlc = 6;
	can_frame.data[0] = 0;
	can_frame.data[1] = 0;
	can_frame.data[2] = locId >> 8;
	can_frame.data[3] = locId;
	can_frame.data[4] = state;            /* Meldung der Lage für Märklin-Geräte.*/
  	can_frame.data[5] = 0;
  	sendCanFrame(canfd, can_frame);

  	can_frame.data[4] = 0xfe - state;     /* Meldung für CdB-Module und Rocrail Feldereignisse. */
  	sendCanFrame(canfd, can_frame);

	//Serial.println("S88-Weichenevent");

	can_frame.cmd = S88_EVENT;
	can_frame.dlc = 8;
	can_frame.data[2] = locId >> 8;
	can_frame.data[3] = locId - (state - 1);
	can_frame.data[4] = 1;
	can_frame.data[5] = 0;
	can_frame.data[6] = 0;
	can_frame.data[7] = 0;
  	sendCanFrame(canfd, can_frame);
	can_frame.data[3] = locId + state;
	can_frame.data[4] = 0;
	can_frame.data[5] = 1;
  	sendCanFrame(canfd, can_frame);
 }

void sendAccessoryFrame(int canfd, CanDevice device, uint32_t locId, bool state, bool response, bool power){

	MCANMSG can_frame;

	can_frame.cmd = SWITCH_ACC;
	can_frame.resp_bit = response;
	can_frame.hash = device.hash;
	can_frame.dlc = 6;
	can_frame.data[0] = 0;
	can_frame.data[1] = 0;
	can_frame.data[2] = locId >> 8;
	can_frame.data[3] = locId;
	can_frame.data[4] = state;
	can_frame.data[5] = power;
	sendCanFrame(canfd, can_frame);
}

void checkS88StateFrame(int canfd, CanDevice device, uint16_t dev_id, uint16_t contact_id){

	MCANMSG can_frame;

	can_frame.cmd = S88_EVENT;
	can_frame.resp_bit = 0;
	can_frame.hash = device.hash;
	can_frame.dlc = 4;
	can_frame.data[0] = dev_id >> 8;
	can_frame.data[1] = dev_id;
	can_frame.data[2] = contact_id >> 8;
	can_frame.data[3] = contact_id;
	sendCanFrame(canfd, can_frame);
}

MCANMSG getCanFrame(int canfd){
/* XXX to be reviewed */
	struct can_frame frm;
	uint32_t rxId;
	MCANMSG can_frame;
	
	memset(&frm, 0, sizeof(frm));
	read(canfd, &frm, sizeof(frm));

	memset(&can_frame, 0, sizeof(can_frame));
	memcpy(&can_frame.data, &frm.data, 8);
	rxId = ntohl(frm.can_id);
	can_frame.cmd = rxId >> 17;
	can_frame.hash = rxId;
	can_frame.resp_bit = bitRead(rxId, 16);

//	Serial.println(canFrameToString(can_frame, false));

	return can_frame;
}

int checkAccessoryFrame(MCANMSG can_frame, uint16_t locIds[], int accNum, bool response){

	uint16_t currentLocId = (can_frame.data[2] << 8) | can_frame.data[3];

	if((can_frame.cmd == SWITCH_ACC) && (can_frame.resp_bit == response)){
		for( int i = 0; i < accNum; i++){
			if(currentLocId == locIds[i]) return i;
		}
	}

	return -1;
}

void statusResponse(int canfd, CanDevice device, int chanel, int success){
	MCANMSG can_frame_out;

  can_frame_out.cmd = SYS_CMD;
  can_frame_out.hash = device.hash;
  can_frame_out.resp_bit = true;
  can_frame_out.dlc = 7;
  can_frame_out.data[0] = device.uid >> 24;
  can_frame_out.data[1] = device.uid >> 16;
  can_frame_out.data[2] = device.uid >> 8;
  can_frame_out.data[3] = device.uid;
  can_frame_out.data[4] = SYS_STAT;
  can_frame_out.data[5] = chanel;
  can_frame_out.data[6] = success;
  can_frame_out.data[7] = 0;

  sendCanFrame(canfd, can_frame_out);
}