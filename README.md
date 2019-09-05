# steam-email-verifier
A POP3 client that verifies Steam e-mail addresses.

## Prerequisites
To compile this, you will need OpenSSL.

## Notes
The provided code will only work with Windows because of the WinSock API and a URLOpenBlockingStream call.
If you would like to use this on Linux, change the socket API and use cURL(pp) instead of URLOpenBlockingStream.

# License
This project is redistributed through the MIT license. For further information, check the license file.
