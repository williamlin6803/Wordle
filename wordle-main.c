/* hw3-main.c */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int total_guesses;
int total_wins;
int total_losses;
char ** words;


int wordle_server( int argc, char ** argv );

int main( int argc, char ** argv )
{
  total_guesses = total_wins = total_losses = 0;
  words = calloc( 1, sizeof( char * ) );
  if ( words == NULL ) { perror( "calloc() failed" ); return EXIT_FAILURE; }

  int rc = wordle_server( argc, argv );

  for ( char ** ptr = words ; *ptr ; ptr++ )
  {
    free( *ptr );
  }
  free( words );

  return rc;
}