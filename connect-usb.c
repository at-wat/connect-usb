
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "usb-serials.h"


int gShutoff = 0;

struct state
{
	struct usblist dev;
	int connected;
	pid_t pid;
};

struct devicelist
{
	char *product;
	char *manufacturer;
	char *idproduct;
	char *idvendor;
	char *command;
};

void ctrlc( int aN )
{
	gShutoff = 1;
	signal( SIGINT, NULL );
}

void CommandExec( char *command )
{
	char **argv;
	char *args[128];
	int quot = 0;
	int esc = 0;

	argv = args;
	*argv = command;
	argv ++;

	for( ; *command; command ++ )
	{
		if( *command == '\\' && !esc )
		{
			esc = 1;
			continue;
		}
		if( *command == '\"' && esc && quot )
		{
			memcpy( command - 1, command, strlen( command ) + 1 );
			command --;
		}
		else if( *command == '\"' && !esc )
		{
			if( quot == 0 )
			{
				quot = 1;
				( *( argv - 1 ) ) ++;
			}
			else
			{
				quot = 0;
				*command = 0;
			}
		}
		if( *command == ' ' && !quot )
		{
			*command = 0;
			*argv = command + 1;
			argv ++;
		}
		esc = 0;
	}
	*argv = NULL;

	//for( argv = args; *argv; argv ++ ) printf("%s || ",*argv);
	//printf("\n");
	execv( args[0], args );
}

int main( int argc, char *argv[] )
{
	int i;
	char def[ 8192 ];
	FILE *fd;
	int len;
	struct usblist lsusb[ 128 ];
	struct devicelist devlist[ 128 ];
	int ndevlist;
	struct state status[ 128 ];
	char dummy = 0;

	if( argc != 2 )
	{
		fprintf( stderr, "USAGE: %s sensors.conf\n", argv[ 0 ] );
		fprintf( stderr, "       %s --list\n", argv[ 0 ] );
		return 1;
	}

	if( strcmp( argv[1], "--list" ) == 0 )
	{
		len = getttylist( lsusb );
		for( len --; len >= 0; len -- )
		{
			fprintf( stderr, "Bus %d Device %d: ID %s:%s '%s' '%s' %s\n", 
					lsusb[len].ibus,
					lsusb[len].idev,
					lsusb[len].idvendor,
					lsusb[len].idproduct,
					lsusb[len].manufacturer,
					lsusb[len].product,
					lsusb[len].ttyname );
		}
		return 1;
	}
	else
	{
		fd = fopen( argv[ 1 ], "r" );
		if( fd == NULL )
		{
			fprintf( stderr, "ERROR: \"%s\" not Found\n", argv[ 1 ] );
			return 0;
		}
		len = fread( def, 1, sizeof( def ), fd );
		def[ len ] = 0;
		fclose( fd );
	}

	printf( "\033[31;4mMonitoring cdc_acm and ftdi_sio devices...\033[0m\033[0K\n" );

	ndevlist = 0;
	{
		char *T;
		char *T_next;

		T = def;
		T_next = strstr( T, "\nT:" );
		while( 1 )
		{
			char *pcmd, *pmanu, *pprod, *pcmd_end;
			char *pidvend, *pidprod;

			if( T_next == NULL ) break;
			T = T_next + 1;

			T_next = strstr( T, "\nT:" );
			if( T_next ) *T_next = 0;

			pcmd = strstr( T, "Cmd=" );
			if( pcmd == NULL ) continue;
			pcmd_end = strchr( pcmd, '\n' );
			if( pcmd_end == NULL ) break;
			*pcmd_end = 0;

			devlist[ ndevlist ].command = pcmd + strlen( "Cmd=" );

			pmanu = strstr( pcmd_end + 1, "Manufacturer=" );
			pprod = strstr( pcmd_end + 1, "Product=" );
			pidvend = strstr( pcmd_end + 1, "ManufacturerId=" );
			pidprod = strstr( pcmd_end + 1, "ProductId=" );

			if( pidvend )
			{
				devlist[ ndevlist ].idvendor = pidvend + strlen( "ManufacturerId=" );
				removelf( pidvend );
			}
			else
			{
				devlist[ ndevlist ].idvendor = &dummy;
			}

			if( pmanu )
			{
				devlist[ ndevlist ].manufacturer = pmanu + strlen( "Manufacturer=" );
				removelf( pmanu );
			}
			else
			{
				devlist[ ndevlist ].manufacturer = &dummy;
			}

			if( pidprod )
			{
				devlist[ ndevlist ].idproduct = pidprod + strlen( "ProductId=" );
				removelf( pidprod );
			}
			else
			{
				devlist[ ndevlist ].idproduct = &dummy;
			}

			if( pprod )
			{
				devlist[ ndevlist ].product = pprod + strlen( "Product=" );
				removelf( pprod );
			}
			else
			{
				devlist[ ndevlist ].product = &dummy;
			}
			ndevlist ++;
		}
	}

	signal( SIGINT, ctrlc );
	for( i = 0; i < 128; i ++ )
	{
		status[i].connected = 0;
	}

	while( !gShutoff )
	{
		int i;

		for( i = 0; i < 128; i ++ )
		{
			if( status[i].connected )
			{
				int state;

				if( waitpid( status[i].pid, &state, WNOHANG ) != 0 )
				{
					status[i].connected = 0;
					printf( "\033[31;4mDriver of %s terminated\033[0m\033[0K\n", status[i].dev.ttyname );
				}
			}
		}

		len = getttylist( lsusb );
		for( len --; len >= 0; len -- )
		{
			int iempty;
			int connected;

			connected = 0;
			iempty = -1;
			for( i = 0; i < 128; i ++ )
			{
				if( !status[i].connected )
				{
					if( iempty == -1 )
					{
						iempty = i;
					}
					continue;
				}
				if( status[i].dev.ibus == lsusb[len].ibus &&
					status[i].dev.idev == lsusb[len].idev )
				{
					connected = 1;
				}
			}
			if( iempty == -1 )
			{
				printf( "\033[31;4mToo many devices\033[0m\033[0K\n" );
				break;
			}
			if( !connected )
			{
				int j;
				for( j = 0; j < ndevlist; j ++ )
				{
					if( ( strlen( devlist[ j ].manufacturer ) == 0 ||
						  strcmp( devlist[ j ].manufacturer, lsusb[ len ].manufacturer ) == 0 ) &&
						( strlen( devlist[ j ].product ) == 0 ||
						  strcmp( devlist[ j ].product, lsusb[ len ].product ) == 0 ) &&
						( strlen( devlist[ j ].idproduct ) == 0 ||
						  strcmp( devlist[ j ].idproduct, lsusb[ len ].idproduct ) == 0 ) &&
						( strlen( devlist[ j ].idvendor ) == 0 ||
						  strcmp( devlist[ j ].idvendor, lsusb[ len ].idvendor ) == 0 ) )
					{
						char cmd[ 512 ];
						char _format[ 512 ];
						char *format;

						printf( "\033[31;4m%s connected on %d/%d\033[0m\033[0K\n", lsusb[len].ttyname, lsusb[len].ibus, lsusb[len].idev );
						
						cmd[0] = 0;
						format = _format;
						strcpy( format, devlist[ j ].command );
						while( 1 )
						{
							char *str;

							str = strstr( format, "%s" );
							if( str == NULL ) break;

							*str = 0;
							strcat( cmd, format );
							strcat( cmd, lsusb[len].ttyname );
							format = str + 2;
						}
						strcat( cmd, format );
						printf( "  \033[31;4m%s\033[0m\033[0K\n", cmd );

						status[iempty].dev = lsusb[len];
						status[iempty].connected = 1;
						if( ( status[iempty].pid = fork() ) != 0 )
						{
							break;
						}

						CommandExec( cmd );

						exit( 1 );
					}
				}
			}
		}

		sleep( 1 );
	}
	printf( "\n\033[31;4mTerminating...\033[0m\033[0K\n" );

	for( i = 0; i < 128; i ++ )
	{
		if( status[i].connected )
		{
			int state;

			kill( status[i].pid, SIGINT );
			if( waitpid( status[i].pid, &state, 0 ) != 0 )
			{
				status[i].connected = 0;
				printf( "\033[31;4mDriver of %s terminated\033[0m\033[0K\n", status[i].dev.ttyname );
			}
		}
	}

	return 1;
}


