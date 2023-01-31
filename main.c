
/*
使用DHT11模块，检测温湿度，在lcd1602上面显示，因为1602显示不是每次都能出来，所以加了串口打印，会更加直观。
*/
#include <reg51.h>
#include "lcd.h"
#include <intrins.h>   //函数需要使用空指令和字符循环位移指令
#include <stdio.h>
#include "LCD1602.h"
#include "Delay.h"
#include "AT24C02.h"
#include "Key.h"
#include "Buzzer.h"

sbit Temp_data=P3^6;	//信号引脚
 
unsigned int rec_dat[4];//接收的温湿度的整数和小数八位
unsigned char rec_dat_lcd0[6];
unsigned char rec_dat_lcd1[6];
unsigned char rec_dat_lcd2[6];
unsigned char rec_dat_lcd3[6];
char TLow,THigh;
unsigned char KeyNum;
void DHT11_delay_us(unsigned char n);
void DHT11_delay_ms(unsigned int z);
void DHT11_start();
unsigned char DHT11_rec_byte();
void DHT11_receive();
void InitUART(void);

//主函数
void main()
{	
	InitUART();	//串口中断初始化
	P1=0xf0;
	InitLcd1602();  //lcd初始化
	LcdShowStr(0,0,"H:");
	LcdShowStr(8,0,"T:");
	THigh=AT24C02_ReadByte(0);	//读取温度阈值数据
	TLow=AT24C02_ReadByte(1);
	if(THigh>50 || TLow<0 || THigh<=TLow)
	{
		THigh=25;			//如果阈值非法，则设为默认值
		TLow=15;
	}
		AT24C02_WriteByte(0,THigh);		//写入到At24C02中保存
		Delay(5);
		AT24C02_WriteByte(1,TLow);
		Delay(5);
		LCD_ShowSignedNum(2,8,THigh,2);	//显示阈值数据
		LCD_ShowSignedNum(2,12,TLow,2);
		LCD_Init();//LCD1602初始化
	
	while(1)
	{
		DHT11_delay_ms(150);
		DHT11_receive();
		
		//这个函数是打印字符函数，起到数值转化的作用。
		sprintf(rec_dat_lcd0,"%d",rec_dat[0]);
		sprintf(rec_dat_lcd1,"%d",rec_dat[1]);
		sprintf(rec_dat_lcd2,"%d",rec_dat[2]);
		sprintf(rec_dat_lcd3,"%d",rec_dat[3]);
		DHT11_delay_ms(100);
		
		//湿度
		LcdShowStr(2,0,rec_dat_lcd0);
		LcdShowStr(4,0,".");
		LcdShowStr(5,0,rec_dat_lcd1);
		LcdShowStr(6,0,"%");
		
		//温度
		LcdShowStr(10,0,rec_dat_lcd2);
		LcdShowStr(12,0,".");
		LcdShowStr(13,0,rec_dat_lcd3);
		LcdShowStr(14,0," C");
			
		//下面通过串口助手打印温度
		//我也为什么他不用中断服务函数，就可以打印，那好需要这个中断干嘛，不是可以直接打印吗？这是我的疑问。
		printf("Humi:%d.%d \n",rec_dat[0],rec_dat[1]);
		printf("Temp:%d.%d °C\n",rec_dat[2],rec_dat[3]);
		KeyNum=Key();
		/*阈值判断及显示*/
		if(KeyNum)
		{
			if(KeyNum==1)	//K1按键，THigh自增
			{
				THigh++;
				if(THigh>50){THigh=50;}
				AT24C02_WriteByte(0,THigh);		//写入到At24C02中保存
			  Delay(5);
			}
			if(KeyNum==2)	//K2按键，THigh自减
			{
				THigh--;
				if(THigh<=TLow){THigh++;}
				AT24C02_WriteByte(0,THigh);		//写入到At24C02中保存
				Delay(5);
			}
			if(KeyNum==3)	//K3按键，TLow自增
			{
				TLow++;
				if(TLow>=THigh){TLow--;}
				AT24C02_WriteByte(1,TLow);
				Delay(5);
			}
			if(KeyNum==4)	//K4按键，TLow自减
			{
				TLow--;
				if(TLow<0){TLow=0;}
				AT24C02_WriteByte(1,TLow);
				Delay(5);
			}
		}
			//用的是江科大的LCD1602代码
   		LCD_ShowSignedNum(2,8,THigh,2);	//显示阈值数据
			LCD_ShowSignedNum(2,12,TLow,2);
			if(rec_dat[2]>=THigh)			//越界判断
		{
			LcdShowStr(2,1,"OV:H");
			Buzzer_Time(300);Delay(300);
		}
		else if(rec_dat[2]<TLow)
		{
			LcdShowStr(2,1,"OV:L");
			Buzzer_Time(600);Delay(300);
		}
		else
		{
			LcdShowStr(2,1,"    ");
		}
	}
}
 
//DHT11起始信号
void DHT11_start()	
{
	Temp_data=1;
	
	DHT11_delay_us(2);//延时us   --2*n+5us
	
	Temp_data=0;
	
	DHT11_delay_ms(20);//延时18ms，以上等待DHT11做出应答信号
	
	Temp_data=1;
	
	DHT11_delay_us(13);//总线由上拉电阻拉高 主机延时31us
}
 
//接收一个字节
unsigned char DHT11_rec_byte()
{	
	unsigned char i,dat;	
	for(i=0;i<8;i++)
	{
			while(!Temp_data);
		  DHT11_delay_us(13);//位数据“0"的格式:50微秒的低电平和 26-28 微秒的高电平
												//位数据“1”的格式为:50微秒的低电平加70微秒的高电平。
			dat<<=1;//dat左移一位
			if(Temp_data==1)
			{
				dat+=1;//判断位数据为“1”
			}
			while(Temp_data);//等待DATA引脚输出低电平1
	}
	return dat;	
}
 
//接收温湿度数据
void DHT11_receive()
{
	unsigned int R_H,R_L,T_H,T_L;
	unsigned char RH,RL,TH,TL,revise;
	
	DHT11_start();//DHT11起始信号
	Temp_data=1;//DATA保持高电平
	if(Temp_data==0)//等待输出80微秒的低电平作为应答信号
	{
		while(Temp_data==0);   //等待拉高     
        DHT11_delay_us(40);  //拉高后延时80us，准备发送40位数据的信号
		
        R_H=DHT11_rec_byte();    //接收湿度高八位  
        R_L=DHT11_rec_byte();    //接收湿度低八位  
        T_H=DHT11_rec_byte();    //接收温度高八位  
        T_L=DHT11_rec_byte();    //接收温度低八位
        revise=DHT11_rec_byte(); //接收校正位
 
        DHT11_delay_us(25);    //结束
 
        if((R_H+R_L+T_H+T_L)==revise)      //校正
        {
            RH=R_H;
            RL=R_L;
            TH=T_H;
            TL=T_L;
	
        } 
        /*数据处理，方便显示*/
        rec_dat[0]=RH;
        rec_dat[1]=RL;
        rec_dat[2]=TH;
        rec_dat[3]=TL;
	}	
}
//延时us   --2*n+5us
void DHT11_delay_us(unsigned char n)
{
    while(--n);
}
 
//延时ms
void DHT11_delay_ms(unsigned int z)
{
   unsigned int i,j;
   for(i=z;i>0;i--)
      for(j=110;j>0;j--);
}

//使用定时器1作为串口波特率发生器

void InitUART(void)
{
	TMOD=0x20;					//定时器1的工作方式2
	SCON=0x40;					//串口通信工作方式1
	REN=1;						//允许接收
	TH1=0xFD,TL1=0xFD;		//波特率=9600
	TI=1;                       //这里一定要注意
	TR1=1;	//定时器1开始计时(允许定时器开始计时)
	EA = 1;                  //打开 总中断系统
}

