
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>

int gShutoff = 0;
struct usblist
{
	int ibus;
	int idev;
	char *product;
	char *manufacturer;
};
struct devicelist
{
	char *product;
	char *manufacturer;
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

	argv = args;
	*argv = command;
	argv ++;

	for( ; *command; command ++ )
	{
		if( *command == '\"' )
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
	}
	*argv = NULL;

//	for( argv = args; *argv; argv ++ ) printf("%s || ",*argv);
//	printf("\n");
	execv( args[0], args );
}

void Connect( char *devname,
			  struct usblist *lsusb, int nlsusb,
			  struct devicelist *devlist, int ndevlist,
			  int ibus, int idev, int *connected, pid_t *pidlist )
{
	int j;
	static struct usblist lsusb_prev[ 128 ];
	static int nlsusb_prev = 0;

	for( j = 0; j < nlsusb; j ++ )
	{
		if( lsusb[ j ].ibus == ibus && lsusb[ j ].idev == idev )
		{
			int k;
			int shown;

			shown = 0;
			for( k = 0; k < nlsusb_prev; k ++ )
			{
				if( lsusb[ j ].ibus == lsusb_prev[ k ].ibus && 
					lsusb[ j ].idev == lsusb_prev[ k ].idev )
				{
					if( strcmp( lsusb[ j ].product, lsusb_prev[ k ].product ) == 0 &&
						strcmp( lsusb[ j ].manufacturer, lsusb_prev[ k ].manufacturer ) == 0 )
					{
						shown = 1;
						break;
					}
				}
			}
			if( !shown )
			{
				// 増えたもののみ詳細を表示
				printf( "\033[31;4m%s connected on %d/%d\033[0m\033[0K\n", devname, ibus, idev );
				printf( " \033[31;4mManufacturer=%s\033[0m\033[0K\n", lsusb[ j ].manufacturer );
				printf( " \033[31;4mProduct=%s\033[0m\033[0K\n", lsusb[ j ].product );
			}
		
			for( k = 0; k < ndevlist; k ++ )
			{
				if( strcmp( devlist[ k ].manufacturer, lsusb[ j ].manufacturer ) == 0 &&
					strcmp( devlist[ k ].product, lsusb[ j ].product ) == 0 )
				{
					char cmd[ 512 ];
					char _format[ 512 ];
					char *format;

					if( shown )
					{
						printf( "\033[31;4m%s connected on %d/%d\033[0m\033[0K\n", devname, ibus, idev );
					}
					cmd[0] = 0;
					format = _format;
					strcpy( format, devlist[ k ].command );

					while( 1 )
					{
						char *str;

						str = strstr( format, "%s" );
						if( str == NULL ) break;

						*str = 0;
						strcat( cmd, format );
						strcat( cmd, devname );
						format = str + 2;
					}
					strcat( cmd, format );
					printf( "  \033[31;4m%s\033[0m\033[0K\n", cmd );

					if( ( *pidlist = fork() ) != 0 )
					{
						break;
					}

					CommandExec( cmd );

					exit( 1 );
				}
			}
			break;
		}
	}
	*connected = 1;
	
	for( j = 0; j < nlsusb; j ++ )
	{
		lsusb_prev[ j ] = lsusb[ j ];
	}
	nlsusb_prev = nlsusb;
}

void CheckDisconnect( int *connected,  pid_t *pidlist, char *type, int option )
{
	int i;
	for( i = 0; i < 32; i ++ )
	{
		int status;

		if( pidlist[ i ] == 0 ) continue;

		if( option == 0 ) printf("Waiting\n");
		if( waitpid( pidlist[ i ], &status, option ) != 0 )
		{
			connected[ i ] = 0;
			pidlist[ i ] = 0;
			printf( "\033[31;4mDriver of /dev/tty%s%d terminated\033[0m\033[0K\n", type, i );
		}
	}
}

int main( int argc, char *argv[] )
{
	int i;
	char devices[ 8192 ];
	char def[ 8192 ];
	FILE *fd;
	int len;
	struct usblist lsusb[ 128 ];
	int nlsusb;
	struct devicelist devlist[ 128 ];
	int ndevlist;
	char dummy = 0;
	pid_t pidlist[2][ 32 ];
	int connected[2][ 32 ];
	int devtype;
	int show_list;

	if( argc != 2 )
	{
		fprintf( stderr, "USAGE: %s sensors.conf\n", argv[ 0 ] );
		fprintf( stderr, "       %s --list\n", argv[ 0 ] );
		return 1;
	}

	if( strcmp( argv[1], "--list" ) == 0 )
	{
		show_list = 1;

		len = 0;
		def[ len ] = 0;
	}
	else
	{
		show_list = 0;

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

	if( !show_list )
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

			if( pmanu )
			{
				char *manu_end;

				devlist[ ndevlist ].manufacturer = pmanu + strlen( "Manufacturer=" );
				manu_end = strchr( pmanu, '\n' );
				if( manu_end ) *manu_end = 0;
			}
			else
			{
				devlist[ ndevlist ].manufacturer = &dummy;
			}

			if( pprod )
			{
				char *prod_end;

				devlist[ ndevlist ].product = pprod + strlen( "Product=" );
				prod_end = strchr( pprod, '\n' );
				if( prod_end ) *prod_end = 0;
			}
			else
			{
				devlist[ ndevlist ].product = &dummy;
			}
			//printf( "%s/%s %s\n", devlist[ ndevlist ].manufacturer, devlist[ ndevlist ].product, devlist[ ndevlist ].command );
			ndevlist ++;
		}
	}

	for( devtype = 0; devtype < 2; devtype ++ )
	{
		for( i = 0; i < 32; i ++ )
		{
			connected[devtype][ i ] = 0;
			pidlist[devtype][ i ] = 0;
		}
	}

	signal( SIGINT, ctrlc );

	while( !gShutoff )
	{
		fd = fopen( "/proc/bus/usb/devices", "r" );
		if( fd == NULL )
		{
			fd = fopen( "/sys/kernel/debug/usb/devices", "r" );
			if( fd == NULL )
			{
				fprintf( stderr, "ERROR: Can not open \"/proc/bus/usb/devices\" \n" );
				fprintf( stderr, "ERROR: Can not open \"/sys/kernel/debug/usb/devices\" \n" );
				fprintf( stderr, "%s needs mounted usbfs\n", argv[0] );
				return 0;
			}
		}
		len = fread( devices, 1, sizeof( devices ), fd );
		devices[ len ] = 0;
		fclose( fd );

		nlsusb = 0;
		{
			char *T;
			char *T_next;

			T = devices;
			T_next = strstr( T, "\nT:" );
			while( 1 )
			{
				char *pbus, *pdev, *pmanu, *pprod;

				if( T_next == NULL ) break;
				T = T_next + 1;

				T_next = strstr( T, "\nT:" );
				if( T_next ) *T_next = 0;

				pbus = strstr( T, "Bus=" );
				pdev = strstr( T, "Dev#=" );
				if( pbus == NULL || pdev == NULL ) continue;

				lsusb[ nlsusb ].ibus = atoi( pbus + 4 );
				lsusb[ nlsusb ].idev = atoi( pdev + 5 );

				pmanu = strstr( T, "Manufacturer=" );
				pprod = strstr( T, "Product=" );

				if( pmanu )
				{
					char *manu_end;

					lsusb[ nlsusb ].manufacturer = pmanu + strlen( "Manufacturer=" );
					manu_end = strchr( pmanu, '\n' );
					if( manu_end ) *manu_end = 0;
				}
				else
				{
					lsusb[ nlsusb ].manufacturer = &dummy;
				}

				if( pprod )
				{
					char *prod_end;

					lsusb[ nlsusb ].product = pprod + strlen( "Product=" );
					prod_end = strchr( pprod, '\n' );
					if( prod_end ) *prod_end = 0;
				}
				else
				{
					lsusb[ nlsusb ].product = &dummy;
				}
				if( show_list )
					printf( " %d:%d/%s/%s/\n", lsusb[ nlsusb ].ibus, lsusb[ nlsusb ].idev, 
							lsusb[ nlsusb ].manufacturer, lsusb[ nlsusb ].product );
				nlsusb ++;
			}
		}
		if( show_list ) break;

		// ttyACM
		{
			DIR *dirs;
			struct dirent *dir;

			dirs = opendir( "/sys/bus/usb/drivers/cdc_acm/" );
			if( dirs ) do
			{
				while( ( dir = readdir( dirs ) ) != NULL )
				{
					if( dir->d_type == DT_LNK && isdigit( dir->d_name[0] ) )
					{
						char d_name[256];
						char *pbus, *pdev, *pdev_end;
						char sysname[512];
						DIR *dirs2;
						struct dirent *dir2;
						int ibus2;
						char device_path[256];
						
						strcpy( d_name, dir->d_name );
						strcpy( device_path, dir->d_name );
						
						pbus = d_name;
						pdev = strchr( pbus, '-' );
						if( pdev == NULL ) continue;

						pdev_end = strchr( device_path, ':' );
						if( pdev_end == NULL ) continue;
						*pdev_end = 0;

						ibus2 = atoi( pbus );

						sprintf( sysname, "/sys/bus/usb/devices/%s/tty/", dir->d_name );
						dirs2 = opendir( sysname );
						if( dirs2 )
						{
							while( ( dir2 = readdir( dirs2 ) ) != NULL )
							{
								for( i = 0; i < 32; i ++ )
								{
									char devname[ 512 ];
									char devname2[ 512 ];
									int ibus, idev;

									if( connected[0][ i ] == 1 ) continue;

									sprintf( devname, "/dev/ttyACM%d", i );
									sprintf( devname2, "ttyACM%d", i );

									if( strcmp( dir2->d_name, devname2 ) == 0 )
									{
										FILE *fd;
										char devnum[512];
										char devnum_data[512];
										int len;
									
										ibus = ibus2;
										sprintf( devnum, "/sys/bus/usb/devices/%s/devnum", device_path );
										fd = fopen( devnum, "r" );
										if( fd == NULL ) continue;
									
										len = fread( devnum_data, 1, sizeof( devnum_data ), fd );
										devnum_data[ len ] = 0;
										idev = atoi( devnum_data );
									
										fclose( fd );

										Connect( devname, lsusb, nlsusb, devlist, ndevlist, 
												ibus, idev, &connected[0][i], &pidlist[0][i] );	
										break;
									}
								}
							}
							closedir( dirs2 );
						}
					}
				}
				closedir( dirs );
			} while( 0 );
		}
		// ttyUSB
		{
			DIR *dirs;
			struct dirent *dir;

			dirs = opendir( "/sys/bus/usb/drivers/ftdi_sio/" );
			if( dirs ) do
			{
				while( ( dir = readdir( dirs ) ) != NULL )
				{
					if( dir->d_type == DT_LNK && isdigit( dir->d_name[0] ) )
					{
						char d_name[256];
						char *pbus, *pdev, *pdev_end;
						char sysname[512];
						DIR *dirs2;
						struct dirent *dir2;
						int ibus2;
						char device_path[256];
						
						strcpy( d_name, dir->d_name );
						strcpy( device_path, dir->d_name );
						
						pbus = d_name;
						pdev = strchr( pbus, '-' );
						if( pdev == NULL ) continue;
						*pdev = 0;
						pdev ++;
						pdev_end = strchr( pdev, ':' );
						if( pdev_end ) *pdev_end = 0;
						
						pdev_end = strchr( device_path, ':' );
						if( pdev_end ) *pdev_end = 0;

						ibus2 = atoi( pbus );

						sprintf( sysname, "/sys/bus/usb/devices/%s/", dir->d_name );
						dirs2 = opendir( sysname );
						if( dirs2 )
						{
							while( ( dir2 = readdir( dirs2 ) ) != NULL )
							{
								for( i = 0; i < 32; i ++ )
								{
									char devname[ 512 ];
									char devname2[ 512 ];
									int ibus, idev;

									if( connected[1][ i ] == 1 ) continue;

									sprintf( devname, "/dev/ttyUSB%d", i );
									sprintf( devname2, "ttyUSB%d", i );

									ibus = -1;
									idev = -1;

									if( strcmp( dir2->d_name, devname2 ) == 0 )
									{
										FILE *fd;
										char devnum[512];
										char devnum_data[512];
										int len;

										ibus = ibus2;
										sprintf( devnum, "/sys/bus/usb/devices/%s/devnum", device_path );
										fd = fopen( devnum, "r" );
										if( fd == NULL ) continue;
									
										len = fread( devnum_data, 1, sizeof( devnum_data ), fd );
										devnum_data[ len ] = 0;
										idev = atoi( devnum_data );
									
										fclose( fd );

										Connect( devname, lsusb, nlsusb, devlist, ndevlist, 
												ibus, idev, &connected[1][i], &pidlist[1][i] );	
										break;
									}
								}
							}
						}
					}
				}
				closedir( dirs );
			} while( 0 );
		}

		// 終了したプロセス監視
		CheckDisconnect( connected[0], pidlist[0], "ACM", WNOHANG );
		CheckDisconnect( connected[1], pidlist[1], "USB", WNOHANG );
		sleep( 1 );
	}
	if( !show_list )
		printf( "\033[31;4mTerminating...\033[0m\033[0K\n" );
	CheckDisconnect( connected[0], pidlist[0], "ACM", WNOHANG );
	CheckDisconnect( connected[1], pidlist[1], "USB", WNOHANG );
	for( i = 0; i < 32; i ++ )
	{
		if( pidlist[0][ i ] != 0 )
		{
			kill( pidlist[0][ i ], SIGINT );
		}
		if( pidlist[1][ i ] != 0 )
		{
			kill( pidlist[1][ i ], SIGINT );
		}
	}
	CheckDisconnect( connected[0], pidlist[0], "ACM", 0 );
	CheckDisconnect( connected[1], pidlist[1], "USB", 0 );

	return 1;
}


