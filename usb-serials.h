#ifdef __cplusplus
extern "C"
{
#endif

struct usblist
{
	int ibus;
	int idev;
	char product[512];
	char manufacturer[512];
	char idproduct[16];
	char idvendor[16];
	char ttyname[512];
};

int getttylist( struct usblist *list );
char *removelf( char *txt );

#ifdef __cplusplus
}
#endif

