#pragma once
#include <openssl/ssl.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <string>
#include <mutex>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "openssl.lib")
#pragma comment(lib, "libssl.lib")

namespace forceinline {
	struct pop3_result {
		pop3_result( ) { }
		pop3_result( std::string_view _response );

		//+OK = true, -ERR = false
		bool success = false;

		//response is the response without +OK/-ERR, raw_response is the unchanged response
		std::string response = "", raw_response = "";
	};

	struct pop3_list_result {
		std::uint32_t email_id = 0;
		std::uint32_t email_size = 0;
	};

	class pop3_ssl {
	public:
		pop3_ssl( ) { };
		pop3_ssl( std::string_view pop3_host, std::string_view pop3_port, std::string_view username, std::string_view password );
		
		~pop3_ssl( );

		void initialize( std::string_view pop3_host, std::string_view pop3_port, std::string_view username, std::string_view password );

		pop3_result connect( );
		pop3_result login( );

		void disconnect( );

		bool is_connected( );

		std::vector< pop3_list_result > get_email_list( );
		
		pop3_result delete_email( std::uint32_t email_id );
		pop3_result get_email( std::uint32_t email_id );
		pop3_result quit( );

	private:
		pop3_result send_pop3_command( std::string_view cmd );
		pop3_result get_pop3_result( );

		bool m_is_connected = false;

		std::string m_pop3_host = "", m_pop3_port = "", m_username = "", m_password = "";

		std::mutex m_mtx;

		SOCKET m_socket = 0;
		WSADATA m_wsa_data = { };

		SSL* m_ssl = nullptr;
		SSL_CTX* m_ssl_ctx = nullptr;

		char m_buffer[ 4096 ] = { };
	};
}