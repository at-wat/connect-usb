#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "usb-serials.h"


int readstring( char *data, int len, char *file, ... )
{
	va_list list;
	char filename[512];
	FILE *fd;

	va_start( list, file );
	vsprintf( filename, file, list );
	va_end( list );

	fd = fopen( filename, "r" );
	if( fd == NULL ) return 0;

	len = fread( data, 1, len, fd );
	data[ len ] = 0;
	
	fclose( fd );

	return len;
}

char *removelf( char *txt )
{
	char *lf;

	lf = strchr( txt, '\n' );
	if( lf )
	{
		*lf = 0;
	}
	return txt;
}

int getttylist( struct usblist *list )
{
	DIR *dirs;
	struct dirent *dir;
	int nlist;
	enum
	{
		TYPE_CDC_ACM,
		TYPE_FTDI_SIO,
		TYPE_MAX
	} type = TYPE_CDC_ACM;
	char devnames[TYPE_MAX][32] = { "ttyACM", "ttyUSB" };

	nlist = 0;
	for( type = 0; type < TYPE_MAX; type ++ )
	{
		switch( type )
		{
		case TYPE_CDC_ACM:
			dirs = opendir( "/sys/bus/usb/drivers/cdc_acm/" );
			break;
		case TYPE_FTDI_SIO:
			dirs = opendir( "/sys/bus/usb/drivers/ftdi_sio/" );
			break;
		default:
			return 0;
			break;
		}
		if( !dirs ) continue;
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

				switch( type )
				{
				case TYPE_CDC_ACM:
					sprintf( sysname, "/sys/bus/usb/devices/%s/tty/", dir->d_name );
					break;
				case TYPE_FTDI_SIO:
					sprintf( sysname, "/sys/bus/usb/devices/%s/", dir->d_name );
					break;
				default:
					return 0;
					break;
				}

				dirs2 = opendir( sysname );
				if( !dirs2 ) continue;
				while( ( dir2 = readdir( dirs2 ) ) != NULL )
				{
					char devname[ 512 ];
					int ibus, idev;
					char devnum[512];
					int len;

					if( strstr( dir2->d_name, devnames[type] ) != dir2->d_name ) continue;
					sprintf( devname, "/dev/%s", dir2->d_name );

					ibus = ibus2;
					len = readstring( devnum, 512, "/sys/bus/usb/devices/%s/devnum", device_path );
					if( !len ) break;
					idev = atoi( devnum );

					list[nlist].idev = idev;
					list[nlist].ibus = ibus;
					strcpy( list[nlist].ttyname, devname );
					len = readstring( list[nlist].product, 512, "/sys/bus/usb/devices/%s/product", device_path );
					if( !len ) break;
					removelf( list[nlist].product );
					readstring( list[nlist].manufacturer, 512, "/sys/bus/usb/devices/%s/manufacturer", device_path );
					if( !len ) break;
					removelf( list[nlist].manufacturer );

					nlist ++;
					break;
				}
				closedir( dirs2 );
			}
		}
	}
	closedir( dirs );

	return nlist;
}


