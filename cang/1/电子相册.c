#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <errno.h>
#include <dirent.h>

int tsfd ;//触摸屏驱动
int dst;  //显示屏驱动

//显示任意位置任意大小的BMP图片
bool showbmp(char *bmppath)	
{
	int w = 0;
	int h = 0;
	int src = open(bmppath , O_RDWR);
	if(src == -1)
	{
		printf("到头了！\n");
	}
	
	lseek(src, 18 ,SEEK_SET);	//获取BMP图片的宽w信息
	read(src, &w, 4);
	lseek(src, 22 ,SEEK_SET);	//获取BMP图片的高h信息
	read(src, &h, 4);
	
	char bmpbuf[w*h*3];
	int  lcdbuf[w*h];
	int  tempbuf[w*h];
	
	dst = open("/dev/fb0",O_RDWR);	//打开显示屏驱动
	if(dst == -1)
	{
		printf("显示屏打开失败！\n");
		return -1;
	}

	lseek(src, 54, SEEK_SET);		//跳过BMP图片头信息字节
	
	int rubbish = (4-(w*3)%4)%4;	//BMP图片字节不能被4整除时，加入的垃圾字节数
	for(int i = 0; i < h; i++)
	{
		read(src, &bmpbuf[w*i*3],w*3);
		lseek(src, rubbish, SEEK_CUR);	//在字节读入时，跳过垃圾字节
	}
		
	for(int i = 0; i < w*h; i++)		//将RGB转换成BGR
	{
		lcdbuf[i] = 0x00<<24 | bmpbuf[i*3+2]<<16 | bmpbuf[i*3+1]<<8 | bmpbuf[i*3];
	}
	
	for(int i = 0; i < w; i++)
	{
		for(int j = 0; j < h; j++)
		{
			tempbuf[(h-1-j)*w+i] = lcdbuf[j*w+i];	//BMP像素点上下反转
		}
	}
		
	//内存映射
	int *mmap_bmp = mmap(NULL, 800*480*4, PROT_READ|PROT_WRITE, MAP_SHARED, dst, 0);

	for(int i = 0; i < h; i++)
	{
		for(int j = 0; j < w; j++)
		{
			//				 自定义y偏移量	自定义x偏移量
			*(mmap_bmp+800*(((480-h)/2)+i)+((800-w)/2)+j) = tempbuf[i*w+j];
		}
	}
	//解除内存映射
	munmap(mmap_bmp, 800*480*4);
	
	close(src);
	close(dst);
}	

//打开触摸屏
int touch_open()	
{	
	tsfd=open("/dev/input/event0",O_RDWR);	//打开触摸屏驱动
	
	if(tsfd==-1)
	{
		perror("打开触摸屏失败!\n");
		return -1;
	}
	return 0;
	
}

int get_xy(int *x, int *y)		//获取触摸屏的坐标值
{
	struct input_event myevent;
	int count = 0;
	while(1)
	{
		read(tsfd,&myevent,sizeof(myevent));
		
		if(myevent.type==EV_ABS) //说明触发的是触摸屏事件
		{
			if(myevent.code==ABS_X) //x坐标
			{
				*x = (myevent.value*800)/1024;
				count ++;
			}
			
			if(myevent.code==ABS_Y) //y坐标
			{
				*y = (myevent.value*480)/600;
				count ++;
			}
		}
		if(count == 2)break;
	}
}	

//关闭触摸屏
int touch_close()	
{
	close(tsfd);
	return 0;
}

//设置节点
struct node
{
	char *data;
	struct node *next;
	struct node *prev;
};

//初始化链表
struct node *list_init()
{
	struct node *head = malloc(sizeof(struct node));
	head->next = head;
	head->prev = head;
	return head;
}

//创建新节点
struct node *newnode(char *newdata)
{
	struct node *new = malloc(sizeof(struct node));
	new->data = newdata;
	new->next = NULL;
	new->prev = NULL;
}

//加入新节点
int addnode(struct node *new,struct node *list)
{
	struct node *p = list;
	while(p->next != list)
	{
		p = p->next;
	}
	new->prev = list->prev;
	new->next = list;
	list->prev = new;
	new->prev->next = new;
}


int main(int argc,char **argv)
{
	struct node *list = list_init();//初始化双向循环链表
	DIR *dp = opendir(argv[1]);		//打开目录文件argv[1]
	struct dirent *p;
	
	while(p = readdir(dp))		//遍历目录文件
	{
		if(p->d_type == DT_REG)
		{
			if(strstr(p->d_name,".bmp")) //判断是否为.bmp文件
			{
				struct node *new = newnode(p->d_name); //创建新节点
				addnode(new,list);  //插入新节点
			}				
		}
	}

	char buf[20];
	struct node *head = list->next;
	
	int tx = 0,ty = 0;
	touch_open();//打开触摸屏
	
	while(1)
	{		
		get_xy(&tx, &ty);//获取触摸屏的坐标
		if(ty > 190 && ty < 290)
		{
			if(tx > 700 && tx < 800)	//右翻
			{
				//printf("你点击的坐标位置X坐标是:%d\n", tx);
				//printf("你点击的坐标位置Y坐标是:%d\n", ty);
				head = head->next;
				bzero(buf,20);
				sprintf(buf,"%s/%s",argv[1],head->data);
			}
			if(tx < 100 && tx > 0)		//左翻
			{
				//printf("你点击的坐标位置X坐标是:%d\n", tx);
				//printf("你点击的坐标位置Y坐标是:%d\n", ty);
				head = head->prev;
				bzero(buf,20);
				sprintf(buf,"%s/%s",argv[1],head->data);
			}
			showbmp("background.bmp");//开发板下，可执行文件目录下存放的背景图（可以使用绝对路径！）
			showbmp(buf);	//显示BMP图片
		}	
		
		if(tx > 700 && ty < 100)
		{
			showbmp("background.bmp");
			break;
		}
	}
	//关闭触摸屏
	touch_close();
}