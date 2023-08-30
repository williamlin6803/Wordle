#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int total_guesses;
int total_wins;
int total_losses;
char ** words;


int wordle_server( int argc, char ** argv );

void print_global_variables() {
    printf("Total guesses: %d\n", total_guesses);
    printf("Total wins: %d\n", total_wins);
    printf("Total losses: %d\n", total_losses);
    
    printf("Words: ");
    if (words) {
        for (char **ptr = words; *ptr; ptr++) {
            printf("%s ", *ptr);
        }
    } else {
        printf("NULL");
    }
    printf("\n");
}

int main( int argc, char ** argv )
{
  total_guesses = total_wins = total_losses = 0;
  words = calloc( 1, sizeof( char * ) );
  if ( words == NULL ) { perror( "calloc() failed" ); return EXIT_FAILURE; }

  int rc = wordle_server( argc, argv );

  print_global_variables();
  for ( char ** ptr = words ; *ptr ; ptr++ )
  {
    free( *ptr );
  }
  free( words );

  return rc;
}