#include "pop3_ssl.hpp"
#include <stdexcept>

namespace forceinline {
	pop3_result::pop3_result( std::string_view _response ) {
		auto ok_response = _response.find( "+OK" );
		auto err_response = _response.find( "-ERR" );

		raw_response = _response;

		if ( ok_response == 0 ) {
			success = true;
			response = _response.substr( 4 );
		} else if ( err_response == 0 ) {
			success = false;
			response = _response.substr( 5 );
		} else
			throw std::exception( "pop3_result::pop3_result: empty or unknown response" );
	}

	pop3_ssl::pop3_ssl( std::string_view pop3_host, std::string_view pop3_port, std::string_view username, std::string_view password ) {
		initialize( pop3_host, pop3_port, username, password );
	}

	pop3_ssl::~pop3_ssl( ) {
		disconnect( );
	}

	void pop3_ssl::initialize( std::string_view pop3_host, std::string_view pop3_port, std::string_view username, std::string_view password ) {
		if ( pop3_host.empty( ) )
			throw std::invalid_argument( "pop3_ssl::initialize: invalid host" );

		if ( pop3_port.empty( ) )
			throw std::invalid_argument( "pop3_ssl::initialize: invalid port" );

		if ( username.empty( ) )
			throw std::invalid_argument( "pop3_ssl::initialize: invalid username" );

		if ( password.empty( ) )
			throw std::invalid_argument( "pop3_ssl::initialize: invalid password" );

		m_pop3_host = pop3_host;
		m_pop3_port = pop3_port;
		m_username = username;
		m_password = password;

		//Startup WinSock
		if ( WSAStartup( MAKEWORD( 2, 2 ), &m_wsa_data ) != 0 )
			throw std::exception( "" );

		//Initialize OpenSSL
		SSL_load_error_strings( );
		SSL_library_init( );
	}

	pop3_result pop3_ssl::connect( ) {
		//Create a TLS SSL context
		m_ssl_ctx = SSL_CTX_new( SSLv23_client_method( ) );

		if ( !m_ssl_ctx )
			throw std::exception( "pop3_ssl::connect: couldn't create SSL context" );

		//Create an SSL object
		m_ssl = SSL_new( m_ssl_ctx );

		if ( !m_ssl )
			throw std::exception( "pop3_ssl::connect: couldn't create SSL object" );

		//Create socket
		m_socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );

		if ( m_socket == INVALID_SOCKET )
			throw std::exception( "pop3_ssl::connect: couldn't create socket" );

		sockaddr_in addr = { };

		struct addrinfo hints, * result;
		ZeroMemory( &hints, sizeof hints );

		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_IP;

		if ( getaddrinfo( m_pop3_host.data( ), m_pop3_port.data( ), &hints, &result ) != 0 )
			throw std::exception( "pop3_ssl::connect: getaddrinfo call failed" );

		//Connect to the pop3 server
		if ( ::connect( m_socket, result->ai_addr, int( result->ai_addrlen ) ) == SOCKET_ERROR )
			throw std::exception( "pop3_ssl::connect: couldn't connect to host" );

		freeaddrinfo( result );

		//Redirect SSL
		if ( SSL_set_fd( m_ssl, m_socket ) == 0 )
			throw std::exception( "pop3_ssl::connect: SSL_set_fd failed" );

		//Try SSL handshake
		if ( SSL_connect( m_ssl ) <= 0 )
			throw std::exception( "pop3_ssl::connect: failed to complete handshake" );

		m_is_connected = true;

		//Return the welcome response
		return get_pop3_result( );
	}

	pop3_result pop3_ssl::login( ) {
		auto user_result = send_pop3_command( "USER " + m_username );

		if ( !user_result.success )
			return user_result;

		return send_pop3_command( "PASS " + m_password );
	}

	void pop3_ssl::disconnect( ) {
		if ( !m_is_connected )
			return;

		//Send quit command
		auto quit_result = quit( );

		m_is_connected = false;

		//Shutdown our connection
		SSL_shutdown( m_ssl );
		
		WSACleanup( );

		if ( !quit_result.success )
			throw std::exception( "pop3_ssl::disconnect: QUIT command was unsuccessful, emails may not be deleted" );
	}

	bool pop3_ssl::is_connected( ) {
		return m_is_connected;
	}

	std::vector< pop3_list_result > pop3_ssl::get_email_list( ) {
		auto list_result = send_pop3_command( "LIST" );
		
		if ( !list_result.success )
			throw std::exception( "pop3_ssl::get_emails: LIST request failed" );

		std::vector< pop3_list_result > email_list = { };
	
		auto list_of_emails = list_result.raw_response;
		do {
			auto next_line = list_of_emails.find( "\r\n" );

			if ( next_line == std::string::npos )
				break;

			//Find where to split our string
			auto line = list_of_emails.substr( 0, next_line );
			auto space = line.find( ' ' );

			//Continue if we have no space
			if ( space == std::string::npos ) {
				list_of_emails = list_of_emails.substr( next_line + 2 );
				continue;
			}
		
			//Continue if the first character isn't a digit
			if ( !isdigit( line[ 0 ] ) ) {
				list_of_emails = list_of_emails.substr( next_line + 2 );
				continue;
			}

			//Get the email ID and email size
			std::uint32_t email_id = std::stoi( line.substr( 0, space ) );
			std::uint32_t email_size = std::stoi( line.substr( space + 1 ) );

			//Add our email to our list
			email_list.push_back( { email_id, email_size } );

			//Continue our string
			list_of_emails = list_of_emails.substr( next_line + 2 );
		} while ( list_of_emails.length( ) > 0 );

		return email_list;
	}

	pop3_result pop3_ssl::delete_email( std::uint32_t email_id ) {
		return send_pop3_command( "DELE " + std::to_string( email_id ) );
	}

	pop3_result pop3_ssl::get_email( std::uint32_t email_id ) {
		return send_pop3_command( "RETR " + std::to_string( email_id ) );
	}

	pop3_result pop3_ssl::quit( ) {		
		return send_pop3_command( "QUIT" );
	}

	pop3_result pop3_ssl::send_pop3_command( std::string_view cmd ) {
		std::string _cmd( cmd );
		_cmd += "\r\n";

		//Lock mutex for multithread support
		std::unique_lock lock( m_mtx );

		//Attempt to send our command
		int bytes_sent = 0;
		std::size_t total_bytes_sent = 0;
		do {
			bytes_sent = SSL_write( m_ssl, _cmd.data( ) + total_bytes_sent, _cmd.length( ) - total_bytes_sent );
			total_bytes_sent += bytes_sent;
		} while ( bytes_sent > 0 && total_bytes_sent < _cmd.length( ) );

		//Disconnect if an error occurred
		if ( bytes_sent <= 0 ) {
			disconnect( );
			return pop3_result( "" );
		}

		return get_pop3_result( );
	}

	pop3_result pop3_ssl::get_pop3_result( ) {
		auto str_occurrences = [ ]( std::string_view string, std::string_view pattern ) -> std::size_t {
			if ( string.empty( ) || pattern.empty( ) )
				return 0;

			std::size_t occurrences = 0;
			for ( std::size_t offset = string.find( pattern ); offset != std::string::npos; offset = string.find( pattern, offset + pattern.length( ) ) )
				occurrences++;

			return occurrences;
		};

		//Try to get response
		std::string response = "";
		std::size_t bytes_received = 0;
		do {
			bytes_received = SSL_read( m_ssl, m_buffer, sizeof m_buffer );
			response.insert( response.end( ), m_buffer, m_buffer + bytes_received );

			//If '\r\n' occurs more than once, it's a multi-line response
			auto is_multiline_response = str_occurrences( response, "\r\n" ) > 1;

			//If it's a multiline response, check for '\r\n.\r\n'
			if ( is_multiline_response ) {
				if ( response.rfind( "\r\n.\r\n" ) == response.length( ) - 5 )
					break;
			} else {
				//Check for '\r\n' at end
				if ( response.rfind( "\r\n" ) == response.length( ) - 2 )
					break;
			}

		} while ( bytes_received > 0 );

		//Disconnect if an error occurred
		if ( bytes_received <= 0 ) {
			disconnect( );
			return pop3_result( "" );
		}

		//Return an answer
		return pop3_result( response );
	}
}