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
	char ttyname[512];
};

int getttylist( struct usblist *list );

#ifdef __cplusplus
}
#endif

