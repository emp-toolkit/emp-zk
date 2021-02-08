#ifndef __UNIDIRIO_H__
#define __UNIDIRIO_H__
#include "emp-tool/emp-tool.h"

namespace emp {
class SubBoolChannel {
public:
	int sock;
	FILE * stream = nullptr;
	bool * buf = nullptr;
	int ptr;
	char * stream_buf = nullptr;
	uint64_t counter = 0;
	uint64_t flushes = 0;
	Hash hash;
	SubBoolChannel (int sock): sock(sock) {
		stream_buf = new char[NETWORK_BUFFER_SIZE];
		buf = new bool[NETWORK_BUFFER_SIZE2];
		stream = fdopen(sock, "wb+");
		memset(stream_buf, 0, NETWORK_BUFFER_SIZE);
		setvbuf(stream, stream_buf, _IOFBF, NETWORK_BUFFER_SIZE);
	}
	~SubBoolChannel() {
		fclose(stream);
		delete[] stream_buf;
		delete[] buf;
	}
};

class SenderSubBoolChannel: public SubBoolChannel {public: 
	SenderSubBoolChannel(int sock): SubBoolChannel(sock) {
		ptr = 0;
	}
	
	void flush() {
		flushes++;
		bool data = true;
		while(ptr!=0)
			send_bool(data);
		fflush(stream);
	}

	void send_bool(bool data) {
		buf[ptr] = data;
		ptr++;
		if(ptr == NETWORK_BUFFER_SIZE2) {
			send_bool_raw(buf, NETWORK_BUFFER_SIZE2);
			ptr = 0;
		}
	}

	void send_bool_raw(const bool * data, int length) {
		uint8_t* tmp_arr = new uint8_t[length/8];
		int cnt = 0;
		
		unsigned long long * data64 = (unsigned long long * )data;
		int i = 0;
		for(; i < length/8; ++i) {
			unsigned long long mask = 0x0101010101010101ULL;
			unsigned long long tmp = 0;
#if defined(__BMI2__)
			tmp = _pext_u64(data64[i], mask);
#else
			// https://github.com/Forceflow/libmorton/issues/6
			for (unsigned long long bb = 1; mask != 0; bb += bb) {
				if (data64[i] & mask & -mask) { tmp |= bb; }
				mask &= (mask - 1);
			}
#endif
			tmp_arr[cnt] = tmp;
			cnt++;
		}
		send_data_raw(tmp_arr, cnt);
		if (8*i != length)
			send_data_raw(data + 8*i, length - 8*i);
		delete[] tmp_arr;
	}

	void send_data_raw(const void * data, int len) {
		hash.put(data, len);
		counter += len;
		int sent = 0;
		while(sent < len) {
			int res = fwrite(sent + (char*)data, 1, len - sent, stream);
			if (res >= 0)
				sent+=res;
			else
				fprintf(stderr,"error: net_send_data %d\n", res);
		}
	}
};

class RecverSubBoolChannel: public SubBoolChannel {public:
	RecverSubBoolChannel(int sock): SubBoolChannel(sock) {
		ptr = NETWORK_BUFFER_SIZE2;
	}
	void flush() {
		flushes++;
		ptr = NETWORK_BUFFER_SIZE2;
	}

	bool recv_bool() {
		if(ptr == NETWORK_BUFFER_SIZE2) {
			recv_bool_raw(buf, NETWORK_BUFFER_SIZE2);
			ptr = 0;
		}
		bool res = buf[ptr];
		ptr++;
		return res;
	}

	void recv_bool_raw(bool * data, int length) {
		uint8_t* tmp_arr = new uint8_t[length/8];
		int cnt = 0;
		recv_data_raw(tmp_arr, length/8);

		unsigned long long * data64 = (unsigned long long *) data;
		int i = 0;
		for(; i < length/8; ++i) {
			unsigned long long mask = 0x0101010101010101ULL;
			unsigned long long tmp = tmp_arr[cnt];
			cnt++;
#if defined(__BMI2__)
			data64[i] = _pdep_u64(tmp, mask);
#else
			data64[i] = 0;
			for (unsigned long long bb = 1; mask != 0; bb += bb) {
				if (tmp & bb) {data64[i] |= mask & (-mask); }
				mask &= (mask - 1);
			}
#endif
		}
		if (8*i != length)
			recv_data_raw(data + 8*i, length - 8*i);
		delete[] tmp_arr;
	}


	void recv_data_raw(void  * data, int len) {
		int sent = 0;
		while(sent < len) {
			int res = fread(sent + (char*)data, 1, len - sent, stream);
			if (res >= 0)
				sent += res;
			else 
				fprintf(stderr,"error: net_send_data %d\n", res);
		}
		hash.put(data, len);
	}
};

class BoolIO { public:
	bool is_server, quiet, is_sender;
	int mysock = 0;
	SenderSubBoolChannel * schannel = nullptr;
	RecverSubBoolChannel * rchannel = nullptr;
	string addr;
	int port;
	int server_listen(int port) {
		int mysocket;
		struct sockaddr_in dest;
		struct sockaddr_in serv;
		socklen_t socksize = sizeof(struct sockaddr_in);
		memset(&serv, 0, sizeof(serv));
		serv.sin_family = AF_INET;
		serv.sin_addr.s_addr = htonl(INADDR_ANY); /* set our address to any interface */
		serv.sin_port = htons(port);           /* set the server port number */    
		mysocket = socket(AF_INET, SOCK_STREAM, 0);
		int reuse = 1;
		setsockopt(mysocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
		if(::bind(mysocket, (struct sockaddr *)&serv, sizeof(struct sockaddr)) < 0) {
			perror("error: bind");
			exit(1);
		}
		if(listen(mysocket, 1) < 0) {
			perror("error: listen");
			exit(1);
		}
		int sock = accept(mysocket, (struct sockaddr *)&dest, &socksize);
		close(mysocket);
		return sock;
	}
	int client_connect(const char * address, int port) {
		int sock;
		struct sockaddr_in dest;
		memset(&dest, 0, sizeof(dest));
		dest.sin_family = AF_INET;
		dest.sin_addr.s_addr = inet_addr(address);
		dest.sin_port = htons(port);

		while(1) {
			sock = socket(AF_INET, SOCK_STREAM, 0);
			if (connect(sock, (struct sockaddr *)&dest, sizeof(struct sockaddr)) == 0)
				break;
			
			close(sock);
			usleep(1000);
		}
		return sock;

	}
	BoolIO(bool is_sender, const char * address, int port, bool quiet = true)
			: quiet(quiet), is_sender(is_sender), port(port) {
		is_server = (address == nullptr);
		if (is_server) {
				mysock = server_listen(port);
		}
		else {
			addr = string(address);
			mysock = client_connect(address, port);
		}
		if(is_sender)
			schannel = new SenderSubBoolChannel(mysock);
		else
			rchannel = new RecverSubBoolChannel(mysock);
		if(not quiet)
			std::cout << "connected\n";	
	}

	~BoolIO() {
		flush();
		if(not quiet) {
			if(schannel != nullptr)
				std::cout <<"Data Sent: \t"<<schannel->counter<<"\n";
			if(rchannel != nullptr)
				std::cout <<"Data Received: \t"<<rchannel->counter<<"\n";
			//std::cout <<"Flushes:\t"<<schannel->flushes<<"\t"<<rchannel->flushes<<"\n";
		}
		if(schannel != nullptr)
			delete schannel;
		if(rchannel != nullptr)
			delete rchannel;
		close(mysock);
	}

	void flush() {
		if(is_sender) {
			schannel->flush();
		} else {
			rchannel->flush();
		}
	}

	uint64_t get_counter() {
		if(is_sender) {
			return schannel->counter;
		} else {
			return rchannel->counter;
		}
	}
	
	block get_hash_block() {
		block res[2];
		if(is_sender) {
			schannel->hash.digest((char *)res);
		} else {
			rchannel->hash.digest((char *)res);
		}
		return res[0];
	}

	
	void send_bool(bool data) {
		schannel->send_bool(data);
	}

	bool recv_bool() {
		return rchannel->recv_bool();
	}
};



}
#endif// __UNIDIRIO_H__
