#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <time.h>

const int TIME_PORT = 27015;
const int MAX_SOCKETS = 60;
const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;

#define SEND_GET 1
#define SEND_HEAD 2
#define SEND_OPTIONS 3
#define SEND_PUT 4
#define SEND_DELETE 5
#define SEND_POST 6
#define SEND_TRACE 7

#define CODE_OK 200
#define MSG_OK "OK"
#define CODE_NOT_FOUND 404
#define MSG_NOT_FOUND "Not Found"
#define CODE_NO_CONTENT 204
#define MSG_NO_CONTENT "No Content"
#define CODE_INTERNAL_ERROR 500
#define MSG_INTERNAL_ERROR "Internal Server Error"
#define CODE_CREATED 210
#define MSG_CREATED "Created"

#define CRLF_STR "\r\n"
#define NEWLINE_STR "\n"
#define CR '\r'
#define SPACE ' '
#define SPACE_STR " "

#define EN "en"
#define FR "fr"
#define HE "he"

#define BUFF_SIZE 1024



typedef struct ResponseDetails
{
	const string serverName = "HTTP Web Server";
	int statusCode;
	string statusCodeMsg;
	string responseMsg;
	string contentType = "text/html";
	int msgLen = 0;

}ResponseDetails;

typedef struct RequestDetails
{
	int sendSubType;
	string URI;
	string lang;
	string httpVers;

}RequestDetails;

typedef struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?
	char buffer[BUFF_SIZE];
	int len;
	RequestDetails requestDetails;
	ResponseDetails detailsForResponse;

}SocketState;

extern SocketState sockets[MAX_SOCKETS];
extern int socketsCount;

bool addSocket(SOCKET id, int what);
void removeSocket(int index);
void updateRequestDetails(int index, int sendSubT, int requestLen);
void updateAndRemoveLangFromPath(int index);
void addLangToURI(int index);
void acceptConnection(int index);
void receiveMessage(int index);
void sendMessage(int index);
void ReadFromFile(char* fileName, int index);
void setHeaders(char sendBuff[], int index);
void handleGet(char* sendBuff, const int index);
void handleHead(char* sendBuff, const int index);
void handleOptions(char* sendBuff, const int index);
void updateOptionsForCase(int index, int statusCode, string statusCodeMsg, string options);
void handleTrace(char* sendBuff, const int index);
void initSocketAfterSend(int index);
void handleDelete(char* sendBuff, const int index);
void handlePost(char* sendBuff, const int index);
void handlePut(char* sendBuff, const int index);
string getTheBodyMsg(int index);
void createNewFile(int index, const char* content);
void overwrittenContentToExistFile(int index, FILE* file, const char* content);