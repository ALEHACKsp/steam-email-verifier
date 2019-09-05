#include "pop3_ssl/pop3_ssl.hpp"
#include <iostream>
#include <regex>
#include <urlmon.h>
#include <sstream>

#pragma comment(lib, "urlmon.lib")

std::string pop3_host = "your pop3 host",
		pop3_port = "your pop3 port", //Usually 995
		pop3_user = "your pop3 user",
		pop3_pass = "your pop3 pass";

int main( ) {
	std::regex steam_email_verification_link_regex( "https:\\/\\/store\\.steampowered\\.com\\/account\\/newaccountverification\\?stoken=.*&creationid=.*", std::regex::icase );

	try {
		/*
		 *	Loop that connects to a POP3 server, tries to verifiy Steam accounts and disconnects again.
		 *	We need to put this into a loop because e-mails only get deleted if we send the QUIT command,
		 *	which will disconnect us from the server and we will have to reconnect again.
		 */
		while ( true ) {
			std::this_thread::sleep_for( std::chrono::seconds( 1 ) );

			//Create a POP3 object with our host, port and login information
			forceinline::pop3_ssl pop3( pop3_host, pop3_port, pop3_user, pop3_pass );

			//Try to connect
			auto connect_result = pop3.connect( );

			if ( !connect_result.success )
				return 1;

			std::cout << "Got welcome message from POP3 server" << std::endl;

			//Try to login
			auto login_result = pop3.login( );

			if ( !login_result.success )
				return 1;

			std::cout << "Successfully logged in as " << pop3_user << std::endl;

			//Get a list of our e-mails
			auto email_list = pop3.get_email_list( );

			if ( email_list.empty( ) ) {
				std::cout << "No e-mails currently in inbox." << std::endl;

				//Disconnect to refresh inbox
				pop3.disconnect( );
				continue;
			}

			std::cout << email_list.size( ) << " e-mail(s) in inbox." << std::endl;

			//Get our e-mail content
			for ( auto email : email_list ) {
				//Get specific e-mail
				auto email_result = pop3.get_email( email.email_id );

				if ( !email_result.success )
					continue;

				//Regex search for the verification link inside our e-mail
				std::smatch regex_match;
				if ( !std::regex_search( email_result.raw_response, regex_match, steam_email_verification_link_regex ) )
					continue;

				//Get the e-mail verification link
				auto email_verification_link = regex_match.str( );

				std::cout << "E-Mail #" << email.email_id << " is a Steam verification e-mail, trying to confirm" << std::endl;

				//Verify the e-mail through a HTTP GET request
				IStream* stream = nullptr;
				if ( URLOpenBlockingStreamA( 0, email_verification_link.data( ), &stream, 0, 0 ) != 0 ) {
					std::cout << "Failed to confirm email #" << email.email_id << std::endl;
					continue;
				}

				std::cout << "E-Mail #" << email.email_id << " confirmed, trying to delete it" << std::endl;

				stream->Release( );

				//Mark the e-mail to be deleted
				auto delete_result = pop3.delete_email( email.email_id );

				if ( !delete_result.success )
					std::cout << "Failed to delete e-mail #" << email.email_id << std::endl;
				else
					std::cout << "Marked e-mail #" << email.email_id << " to be deleted" << std::endl;
			}

			std::cout << "Disconnecting from POP3 server" << std::endl;

			//Disconnect to refresh inbox and to delete marked e-mails.
			pop3.disconnect( );
		}
	} catch ( const std::exception& e ) {
		std::cout << e.what( ) << std::endl;
	}

	std::cin.get( );
	return 0;
}