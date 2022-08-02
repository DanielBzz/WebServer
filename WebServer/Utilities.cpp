#include "Utilities.h"

SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;

bool addSocket(SOCKET id, int what)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].len = 0;
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

void removeSocket(int index)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	socketsCount--;
}

void acceptConnection(int index)
{
	SOCKET id = sockets[index].id;
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Time Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Time Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "Time Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}

	return;
}

void receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Time Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "Time Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";

		sockets[index].len += bytesRecv;

		if (sockets[index].len > 0)
		{
			if (strncmp(sockets[index].buffer, "GET", 3) == 0)
			{
				updateRequestDetails(index, SEND_GET, 4);
			}
			else if (strncmp(sockets[index].buffer, "HEAD", 4) == 0)
			{
				updateRequestDetails(index, SEND_HEAD, 5);
			}
			else if (strncmp(sockets[index].buffer, "OPTIONS", 7) == 0)
			{
				updateRequestDetails(index, SEND_OPTIONS, 8);
			}
			else if (strncmp(sockets[index].buffer, "PUT", 3) == 0)
			{
				updateRequestDetails(index, SEND_PUT, 4);
			}
			else if (strncmp(sockets[index].buffer, "DELETE", 6) == 0)
			{
				updateRequestDetails(index, SEND_DELETE, 7);
			}
			else if (strncmp(sockets[index].buffer, "POST", 4) == 0)
			{
				updateRequestDetails(index, SEND_POST, 5);
			}
			else if (strncmp(sockets[index].buffer, "TRACE", 5) == 0)
			{
				updateRequestDetails(index, SEND_TRACE, 6);
			}
			else if (strncmp(sockets[index].buffer, "EXIT", 4) == 0)
			{
				closesocket(msgSocket);
				removeSocket(index);
				return;
			}
		}
	}
}

void sendMessage(int index)
{
	int bytesSent = 0, ind = index;
	char sendBuff[BUFF_SIZE];

	SOCKET msgSocket = sockets[index].id;

	switch (sockets[index].requestDetails.sendSubType)
	{
	case SEND_GET:
		handleGet(sendBuff, ind);
		break;
	case SEND_HEAD:
		handleHead(sendBuff, ind);
		break;
	case SEND_OPTIONS:
		handleOptions(sendBuff, ind);
		break;

	case SEND_PUT:
		handlePut(sendBuff, ind);
		break;

	case SEND_DELETE:
		handleDelete(sendBuff, ind);
		break;

	case SEND_POST:
		handlePost(sendBuff, ind);
		break;

	case SEND_TRACE:
		handleTrace(sendBuff, ind);
		break;
	}

	bytesSent = send(msgSocket, sendBuff, (int)strlen(sendBuff), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Time Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "Time Server: Sent: " << bytesSent << "\\" << strlen(sendBuff) << " bytes of \"" << sendBuff << "\" message.\n";
	initSocketAfterSend(index);
}

void initSocketAfterSend(int index)
{
	sockets[index].send = IDLE;
	memcpy(sockets[index].buffer, "", 0);
	sockets[index].len = 0;
	sockets[index].detailsForResponse.responseMsg.clear();
	sockets[index].detailsForResponse.msgLen = 0;
	sockets[index].detailsForResponse.statusCode = 0;
	sockets[index].detailsForResponse.statusCodeMsg.clear();
	sockets[index].requestDetails.httpVers.clear();
	sockets[index].requestDetails.URI.clear();
	sockets[index].requestDetails.lang.clear();
	sockets[index].requestDetails.sendSubType = 0;
	sockets[index].detailsForResponse.contentType = "text/html";
}

void updateRequestDetails(int index, int sendSubT, int requestLen)
{
	sockets[index].send = SEND;
	sockets[index].requestDetails.sendSubType = sendSubT;

	int i = 1 + requestLen;
	for (i; i < sockets[index].len; ++i) // starting from 1 + request len to remove the request name and first '/'
	{
		if (sockets[index].buffer[i] == SPACE)
		{
			break;
		}

		sockets[index].requestDetails.URI.push_back(sockets[index].buffer[i]);
	}

	for (i++; i < sockets[index].len; ++i) // ++ to remove the space
	{
		if (sockets[index].buffer[i] == CR)
		{
			break;
		}

		sockets[index].requestDetails.httpVers.push_back(sockets[index].buffer[i]);
	}

	updateAndRemoveLangFromPath(index);
}

void updateAndRemoveLangFromPath(int index)
{
	int pos = sockets[index].requestDetails.URI.find('?');

	if (pos != string::npos && strncmp(sockets[index].requestDetails.URI.c_str() + pos + 1, "lang=", 5) == 0)
	{
		if (strncmp(sockets[index].requestDetails.URI.c_str() + pos + 6, FR, 2) == 0)
		{
			sockets[index].requestDetails.lang = FR;

		}
		else if (strncmp(sockets[index].requestDetails.URI.c_str() + pos + 6, HE, 2) == 0)
		{
			sockets[index].requestDetails.lang = HE;
		}
		else		// ENGLISH IS DEFAULT
		{
			sockets[index].requestDetails.lang = EN;
		}
		sockets[index].requestDetails.URI.erase(pos, sockets[index].requestDetails.URI.length() - pos);
	}
	else
	{
		sockets[index].requestDetails.lang = EN;
	}
}

void addLangToURI(int index)
{
	if (sockets[index].requestDetails.lang == HE || sockets[index].requestDetails.lang == FR)
	{
		string tempPath(sockets[index].requestDetails.URI);
		int pos = tempPath.find('.');
		string suffix;
		if (pos != string::npos)
		{
			suffix = tempPath.substr(pos, tempPath.length() - pos);
			tempPath.erase(pos, tempPath.length() - pos);
		}

		tempPath.append("(");
		tempPath.append(sockets[index].requestDetails.lang);
		tempPath.append(")");
		tempPath.append(suffix);

		FILE* file = fopen(tempPath.c_str(), "r");
		if (file != NULL)
		{
			sockets[index].requestDetails.URI = tempPath;	// if the file opened => we have the file in that language , else keep it in default english
			fclose(file);
		}
		else
		{
			sockets[index].requestDetails.lang = EN;
		}
	}
}

void ReadFromFile(const char* fileName, int index)
{
	FILE* file = fopen(fileName, "r");
	char line[128];

	if (file == NULL)
	{
		sockets[index].detailsForResponse.statusCode = CODE_NOT_FOUND;
		sockets[index].detailsForResponse.statusCodeMsg = MSG_NOT_FOUND;

		return;
	}

	sockets[index].detailsForResponse.statusCode = CODE_OK;
	sockets[index].detailsForResponse.statusCodeMsg = MSG_OK;

	int counter = 0;

	while (!feof(file))
	{
		fgets(line, 128, file);
		sockets[index].detailsForResponse.responseMsg.append(line);
	}

	sockets[index].detailsForResponse.msgLen = sockets[index].detailsForResponse.responseMsg.length();
}

void setHeaders(char sendBuff[], const int index)
{
	time_t timer;
	time(&timer);
	char line[5];

	strncpy(sendBuff, sockets[index].requestDetails.httpVers.c_str(), BUFF_SIZE);
	strncat(sendBuff, SPACE_STR, BUFF_SIZE);
	_itoa(sockets[index].detailsForResponse.statusCode, line, 10);
	strncat(sendBuff, line, BUFF_SIZE);
	strncat(sendBuff, SPACE_STR, BUFF_SIZE);
	strncat(sendBuff, sockets[index].detailsForResponse.statusCodeMsg.c_str(), BUFF_SIZE);
	strncat(sendBuff, NEWLINE_STR, BUFF_SIZE);
	strncat(sendBuff, "Server: ", BUFF_SIZE);
	strncat(sendBuff, sockets[index].detailsForResponse.serverName.c_str(), BUFF_SIZE);
	strncat(sendBuff, NEWLINE_STR, BUFF_SIZE);
	
	if (sockets[index].requestDetails.sendSubType == SEND_OPTIONS)
	{
		strncat(sendBuff, "Allow: ", BUFF_SIZE);
		strncat(sendBuff, sockets[index].detailsForResponse.responseMsg.c_str(), BUFF_SIZE);
		strncat(sendBuff, NEWLINE_STR, BUFF_SIZE);
	}

	strncat(sendBuff, "Content-Type: ", BUFF_SIZE);
	strncat(sendBuff, sockets[index].detailsForResponse.contentType.c_str(), BUFF_SIZE);
	strncat(sendBuff, NEWLINE_STR, BUFF_SIZE);
	strncat(sendBuff, "Date: ", BUFF_SIZE);
	strncat(sendBuff, ctime(&timer), BUFF_SIZE);
	strncat(sendBuff, "Content-Length: ", BUFF_SIZE);
	_itoa(sockets[index].detailsForResponse.msgLen, line, 10);
	strncat(sendBuff, line, BUFF_SIZE);
	strncat(sendBuff, NEWLINE_STR, BUFF_SIZE);
	strncat(sendBuff, CRLF_STR, BUFF_SIZE);
}

void handleGet(char* sendBuff, const int index)
{
	addLangToURI(index);
	ReadFromFile(sockets[index].requestDetails.URI.c_str(), index);
	setHeaders(sendBuff, index);
	strncat(sendBuff, sockets[index].detailsForResponse.responseMsg.c_str(), BUFF_SIZE);
}

void handleHead(char* sendBuff, const int index)
{
	ReadFromFile(sockets[index].requestDetails.URI.c_str(), index);
	setHeaders(sendBuff, index);
}

void handleOptions(char* sendBuff, const int index)
{
	if (sockets[index].requestDetails.URI == "*")
	{
		updateOptionsForCase(index, CODE_OK, MSG_OK, "OPTIONS, GET, HEAD, POST, PUT, DELETE, TRACE");
	}
	else
	{
		updateAndRemoveLangFromPath(index);
		addLangToURI(index);
		FILE* file = fopen(sockets[index].requestDetails.URI.c_str(), "r");
		if (file != NULL)
		{
			updateOptionsForCase(index, CODE_OK, MSG_OK, "OPTIONS, GET, HEAD, POST, PUT, DELETE, TRACE");
		}
		else
		{
			updateOptionsForCase(index, CODE_NO_CONTENT, MSG_NO_CONTENT, "OPTIONS, PUT, TRACE");
		}
	}

	setHeaders(sendBuff, index);
}

void updateOptionsForCase(int index, int statusCode, string statusCodeMsg, string options)
{
	sockets[index].detailsForResponse.statusCode = statusCode;
	sockets[index].detailsForResponse.statusCodeMsg = statusCodeMsg;
	sockets[index].detailsForResponse.responseMsg = options;
}

void handleTrace(char* sendBuff, const int index)
{
	sockets[index].detailsForResponse.statusCode = CODE_OK;
	sockets[index].detailsForResponse.statusCodeMsg = MSG_OK;
	sockets[index].detailsForResponse.responseMsg = sockets[index].buffer;
	sockets[index].detailsForResponse.msgLen = sockets[index].detailsForResponse.responseMsg.length();
	sockets[index].detailsForResponse.contentType = "message/http";
	setHeaders(sendBuff, index);
	strncat(sendBuff, sockets[index].detailsForResponse.responseMsg.c_str(), BUFF_SIZE);
}

void handleDelete(char* sendBuff, const int index)
{
	updateAndRemoveLangFromPath(index);
	addLangToURI(index);

	FILE* file = fopen(sockets[index].requestDetails.URI.c_str(), "r");
	if (file != NULL)
	{
		sockets[index].detailsForResponse.statusCode = CODE_OK;
		sockets[index].detailsForResponse.statusCodeMsg = MSG_OK;
		fclose(file);
		if (!(remove(sockets[index].requestDetails.URI.c_str()) == 0))		// if it failed
		{
			sockets[index].detailsForResponse.statusCode = CODE_INTERNAL_ERROR;
			sockets[index].detailsForResponse.statusCodeMsg = MSG_INTERNAL_ERROR;
		}
		else
		{
			sockets[index].detailsForResponse.responseMsg = sockets[index].requestDetails.URI.c_str();
			sockets[index].detailsForResponse.responseMsg.append(" deleted");
			sockets[index].detailsForResponse.msgLen = sockets[index].detailsForResponse.responseMsg.length();
		}
	}
	else
	{
		sockets[index].detailsForResponse.statusCode = CODE_NOT_FOUND;
		sockets[index].detailsForResponse.statusCodeMsg = MSG_NOT_FOUND;
	}

	setHeaders(sendBuff, index);
	strncat(sendBuff, sockets[index].detailsForResponse.responseMsg.c_str(), BUFF_SIZE);
}

void handlePut(char* sendBuff, const int index)
{
	updateAndRemoveLangFromPath(index);
	addLangToURI(index);

	string content = getTheBodyMsg(index);
	FILE* file = fopen(sockets[index].requestDetails.URI.c_str(), "r+");
	if (file == NULL)
	{
		createNewFile(index, content.c_str());
	}
	else	// in case the file exist we appending the content (not overwritten)
	{
		overwrittenContentToExistFile(index, file, content.c_str());
		sockets[index].detailsForResponse.responseMsg = "The content appended in ";
		sockets[index].detailsForResponse.responseMsg.append(sockets[index].requestDetails.URI);
	}

	sockets[index].detailsForResponse.msgLen = sockets[index].detailsForResponse.responseMsg.length();
	setHeaders(sendBuff, index);
	strncat(sendBuff, sockets[index].detailsForResponse.responseMsg.c_str(),BUFF_SIZE);
}

void handlePost(char* sendBuff, const int index)
{
	string content = getTheBodyMsg(index);

	cout << content << endl;

	sockets[index].detailsForResponse.statusCode = CODE_NO_CONTENT;
	sockets[index].detailsForResponse.statusCodeMsg = MSG_NO_CONTENT;
	setHeaders(sendBuff, index);
}

string getTheBodyMsg(int index)
{
	int i = 0;
	string body;

	for (i; i < sockets[index].len - 2; ++i) // get the body
	{
		if (sockets[index].buffer[i] == '\n' && sockets[index].buffer[i + 1] == '\r' && sockets[index].buffer[i + 2] == '\n')
		{
			body.append(sockets[index].buffer + i + 3, strlen(sockets[index].buffer) - (i + 3));
		}
	}

	return body;
}

void createNewFile(int index, const char* content)
{
	FILE* file = fopen(sockets[index].requestDetails.URI.c_str(), "w");
	if (file == NULL)
	{
		sockets[index].detailsForResponse.statusCode = CODE_INTERNAL_ERROR;
		sockets[index].detailsForResponse.statusCodeMsg = MSG_INTERNAL_ERROR;
		return;
	}

	sockets[index].detailsForResponse.statusCode = CODE_CREATED;
	sockets[index].detailsForResponse.statusCodeMsg = MSG_CREATED;
	fputs(content, file);
	sockets[index].detailsForResponse.responseMsg = sockets[index].requestDetails.URI;
	fclose(file);
}

void overwrittenContentToExistFile(int index, FILE* file, const char* content)
{
	fseek(file, 0, SEEK_END);
	fputs(content, file);
	sockets[index].detailsForResponse.statusCode = CODE_OK;
	sockets[index].detailsForResponse.statusCodeMsg = MSG_OK;
	fclose(file);
}
