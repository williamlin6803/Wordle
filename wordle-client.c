#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>

int main()
{
  /* create TCP client socket (endpoint) */
  int sd = socket( AF_INET, SOCK_STREAM, 0 );
  if ( sd == -1 ) { perror( "socket() failed" ); exit( EXIT_FAILURE ); }


  struct hostent * hp = gethostbyname( "127.0.0.1" );


  if ( hp == NULL )
  {
    fprintf( stderr, "ERROR: gethostbyname() failed\n" );
    return EXIT_FAILURE;
  }

  struct sockaddr_in tcp_server;
  tcp_server.sin_family = AF_INET;  /* IPv4 */
  memcpy( (void *)&tcp_server.sin_addr, (void *)hp->h_addr, hp->h_length );
  unsigned short server_port = 8192;
  tcp_server.sin_port = htons( server_port );

  printf( "CLIENT: TCP server address is %s\n", inet_ntoa( tcp_server.sin_addr ) );

  printf( "CLIENT: connecting to server...\n" );

  if ( connect( sd, (struct sockaddr *)&tcp_server, sizeof( tcp_server ) ) == -1 )
  {
    perror( "connect() failed" );
    return EXIT_FAILURE;
  }


  /* The implementation of the application protocol is below... */

while ( 1 )    /* TO DO: fix the memory leaks! */
{
  char * buffer = calloc( 9, sizeof( char ) );
  if ( fgets( buffer, 9, stdin ) == NULL ) break;
  *(buffer + 5) ='\0';   

  printf( "CLIENT: Sending to server: %s\n", buffer );
  int n = write( sd, buffer, strlen( buffer ) );    /* or use send()/recv() */
  if ( n == -1 ) { perror( "write() failed" ); return EXIT_FAILURE; }

  n = read( sd, buffer, 9 );    /* BLOCKING */

  if ( n == -1 )
  {
    perror( "read() failed" );
    return EXIT_FAILURE;
  }
  else if ( n == 0 )
  {
    printf( "CLIENT: rcvd no data; TCP server socket was closed\n" );
    break;
  }
  else /* n > 0 */
  {
    switch ( *buffer )
    {
      case 'N': printf( "CLIENT: invalid guess -- try again" ); break;
      case 'Y': printf( "CLIENT: response: %s", buffer + 3 ); break;
      default: break; /* ?!?! */
    }

    short guesses = ntohs( *(short *)(buffer + 1) );
    printf( " -- %d guess%s remaining\n", guesses, guesses == 1 ? "" : "es" );
    if ( guesses == 0 ) break;
  }
}


  printf( "CLIENT: disconnecting...\n" );

  close( sd );

  return EXIT_SUCCESS;
}