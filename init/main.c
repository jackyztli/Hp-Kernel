#define NULL 0
unsigned char get_fontdata(int index);
void putchar(unsigned int * fb,int Xsize,int x,int y,unsigned int FRcolor,unsigned int BKcolor,unsigned char font)
{
	int i = 0,j = 0;
	unsigned int *addr = NULL;
	int testval = 0;

	for(i = 0; i<16;i++)
	{
		addr = fb + Xsize * ( y + i ) + x;
		testval = 0x100;
		for(j = 0;j < 8;j ++)
		{
			testval = testval >> 1;
			if(get_fontdata(font * 16 + i) & testval)
				*addr = FRcolor;
			else
				*addr = BKcolor;
			addr++;
		}
	}
}

void puts(const char *str)
{
	unsigned int *addr = (unsigned int *)0xffff800000a00000;

	const char *s = str;
	int i = 0;
	int j = 0;
	while (*s != '\0') {
		putchar(addr, 1440, i * 8, j * 16, 0x00ffffff, 0x00000000, *s);
		s++;
		i++;
		j++;
	}
}

void kernel_init(void)
{
    puts("ABCDEFGHIJKLMNOPQ,RSTUVWSYZ:zyswvutsr,qponmlkjihgfedcba");
    while (1) {

    }
}
