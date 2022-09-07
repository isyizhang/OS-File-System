#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <fcntl.h>
#include <ctype.h>
#include<windows.h>

FILE *f_sy;							//用于得到模拟磁盘的文件
int path[5] = { 0,-1,-1,-1,-1 };	//记录到当前所指的索引节点块号的查询路径 

void SETCOLOR(unsigned short ForeColor=7,unsigned short BackGroundColor=0)

{

HANDLE handle=GetStdHandle(STD_OUTPUT_HANDLE);//获取当前窗口句柄

SetConsoleTextAttribute(handle,ForeColor+BackGroundColor*0x10);//设置颜色

}
//读取指定块
int GETBLK(char buf[], int blk_no)  
{
	int i = 0;
	if (blk_no < 0 || blk_no >= 1024)
		return -1;
	fseek(f_sy, blk_no * 1024, 0);
	for (i = 0; i < 1024; i++)
		buf[i] = fgetc(f_sy);
	return 0;
}
//写入指定块
int PUTBLK(char buf[], int blk_no)  
{
	int i = 0;
	if (blk_no < 0 || blk_no >= 1024)
		return -1;
	fseek(f_sy, blk_no * 1024, 0);
	for (i = 0; i < 1024; i++)
		fputc(buf[i], f_sy);
	return 0;
}

void INITFILESYSTEM(FILE *f)
{
	char buf[1024] = { 0 };
	for (int i = 1024 * 1024; i >= 1; --i)
		fputc('0', f_sy);

	//第0块为 Boot Block 
	memset(buf, 0, sizeof(buf));
	strcpy(buf,"You are launching a file system made by XU Xiaojia and XIAO Ruojin.");
	PUTBLK(buf, 0);

	//第1块为 Super Block 
	memset(buf, 0, sizeof(buf));
	strcpy(buf, "The size of file system: 1MB \nBlock size: 1KB \nBlock number: 1024 \n"
               "0 block: boot block \n1st block: super block \n 2nd block: block bitmap \n3rd block: inode bitmap \n"
               "4th-8th block: inode table \nother blocks: data block\n");
	PUTBLK(buf, 1);

	//第2块为 Data block bitmap
	GETBLK(buf, 2);
	buf[9] = '1';
	PUTBLK(buf, 2);

	//第3块为 Inode bitmap
	GETBLK(buf, 3);
	buf[0] = '1';
	PUTBLK(buf, 3);	
	
	//第4块开始为inode table 
	memset(buf, 0, sizeof(buf));
	strcpy(buf, "0|Root|9|2018-5-16|");
	PUTBLK(buf, 4);

	//第9块为根目录的数据块 
	memset(buf, 0, sizeof(buf));
	strcpy(buf, "0|");
	PUTBLK(buf, 9);
}

//从inode中获取起始块号
int GETSTARTNUM(char buf[1024], int phs)
{
	int i = 2, calcu = 0;
	int loca;
	char num[5] = { 0 };
	
	while (buf[phs + i++] != '|');
	while (buf[phs + i] != '|') 
	{ 
		num[calcu++] = buf[phs + i++]; 
	}
	num[calcu] = '\0';
	loca = atoi(num);
	
	return loca;
} 

//查找空闲索引节点，存放在free_a中 
int TAKEINOD(int free_a[])   
{
	char buf[1024] = { 0 };
	//第3块inode bitmap 
	GETBLK(buf, 3);
	for (int i = 0; i < 80; ++i)
	{
		if (buf[i] == '0')
		{
			free_a[0] = 4 + i / 16;  //索引节点所在块号
			free_a[1] = i % 16 * 64; //索引节点所在块中的第几字节
			return 0;
		}
	}
	free_a[0] = free_a[1] = -1;
	return -1;
}

//查找num个空闲数据块，将其block序号存放在free_a中 
int TAKEDNOD(int num, int free_a[])   
{
	int calcu = 0;
	char buf[1024] = { 0 };
	//第2块data block bitmap 
	GETBLK(buf, 2);
	for (int i = 9; i < 1024; i++)
		if (buf[i] == '0')
		{
			free_a[calcu++] = i;
			if (calcu == num)
				return 0;
		}
	for (; calcu < num; ++calcu)
		free_a[calcu] = -1;   //返回数组
	return -1;
}

//删除文件，置bitmap为'0' 
int DELETINODE(int ino)   
{
	//计算ino对应的物理地址
	int phsc[2] = { 0 };
	phsc[0] = 4 + ino / 16;  //块号 
	phsc[1] = ino % 16 * 64; //起始位置 

	//置此索引节点位图为'0' 
	char buf[1024] = { 0 };
	GETBLK(buf, 3);
	buf[ino] = '0';
	PUTBLK(buf, 3);

	int i = 2, size = 0;
	GETBLK(buf, phsc[0]);
	//是普通文件的索引节点
	if (buf[phsc[1]] == '1')
	{
		while (buf[phsc[1] + i++] != '|'); //跳过文件名 
		size = buf[phsc[1] + i] - '0'; 
		i += 2;

		int calcu_a = 0, calcu_b = 0;
		char b[1024] = { 0 };
		char num[5] = { 0 };
		
		//置数据位图相应位为'0' (最多8位)
		GETBLK(b, 2);
		while (calcu_a++ < size)
		{
			calcu_b = 0;
			while (buf[phsc[1] + i] != '|') 
			{ 
				num[calcu_b++] = buf[phsc[1] + i++]; 
			}
			++i;
			num[calcu_b] = '\0';
			b[atoi(num)] = '0';
		}
		PUTBLK(b, 2);
	}
	//是目录文件的索引节点
	else
	{
		while (buf[phsc[1] + i++] != '|');
		char num[5] = { 0 };
		int calcu = 0;
		while (buf[phsc[1] + i] != '|') 
		{
			num[calcu++] = buf[phsc[1] + i++];
		}
		num[calcu] = '\0';

		//置目录文件数据位图相应位为'0' 
		char b[1024] = { 0 };
		GETBLK(b, 2);
		b[atoi(num)] = '0';
		PUTBLK(b, 2);
		
		//获取目录文件data block，删除其中文件的索引节点 
		GETBLK(b, atoi(num));
		calcu = i = 0;
		while (b[i] != '|') 
		{
			num[calcu++] = b[i++]; 
		}
		++i;
		num[calcu] = '\0';
		int size = atoi(num);
		int calcu_a = 0, calcu_b = 0;
		while (calcu_a++ < size)
		{
			calcu_b = 0;
			while (b[i++] != ',');
			while (b[i] != '|') 
			{
				num[calcu_b++] = b[i++]; 
			}
			++i;
			num[calcu_b] = '\0';
			DELETINODE(atoi(num));	//递归删除 
		}
	}
	return 0;
}

//删除文件的inode 
int DELETEININO(int ino, char filename[])    
{
	//计算ino对应的物理地址
	int phsc[2] = { 0 };
	phsc[0] = 4 + ino / 16;
	phsc[1] = ino % 16 * 64;

	int i = 2, calcu = 0;
	int loca; 
	char buf[1024] = { 0 };
	GETBLK(buf, phsc[0]);
	loca = GETSTARTNUM(buf, phsc[1]);

	char num[5] = { 0 };
	char b[1024] = { 0 };
	GETBLK(b, loca);
	calcu = i = 0;
	while (b[i] != '|') 
	{ 
		num[calcu++] = b[i++]; 
	}
	++i;
	num[calcu] = '\0';
	
	int size = atoi(num);
	int calcu_a = 0, calcu_b = 0;
	char filen[10] = { 0 };
	while (calcu_a++ < size)
	{
		calcu_b = 0;
		memset(filen, 0, sizeof(filen));
		//获取目录中的文件名 
		while (b[i] != ',') 
		{ 
			filen[calcu_b++] = b[i++]; 
		}
		++i;
		
		//找到了指定的待删除的文件 
		if (strcmp(filename, filen) == 0)
		{
			calcu_b = 0;
			while (b[i] != '|') 
			{ 
				num[calcu_b++] = b[i++]; 
			}
			num[calcu_b] = '\0';
			DELETINODE(atoi(num));	//置位图为0 

			//删除目录的data block中该文件的记录 
			char use1[1024] = { 0 };
			strncpy(use1, b + i + 1, sizeof(use1));
			for (--i; b[i] != '|'; --i); 
			++i;
			strncpy(b + i, use1,sizeof(b) - i );

			i = calcu = 0;
			while (b[i] != '|') 
			{ 
				num[calcu++] = b[i++]; 
			}
			strncpy(use1, b + i,sizeof(use1));
			num[calcu] = '\0';
			int size = atoi(num);
			sprintf(num, "%d\0", --size);
			strncpy(b,num,sizeof(b));
			strcat(b, use1);
			PUTBLK(b, loca);
			return 0;
		}
		while (b[i++] != '|');
	}
	
	//未找到指定文件 
	printf("		This file or directory does not exists!\n");
	return -1;
}

//关闭文件
void CLOSEFILE()     
{
	int i = 0;
	for (; i < sizeof(path) && path[i] != -1; ++i);
	path[i - 1] = -1;
	return;
}

//记录datablk[8]
int GETDATABLK(int ino, char filename[], int datablk[])   
{
	int phsc[2] = { 0 };
	phsc[0] = 4 + ino / 16;
	phsc[1] = ino % 16 * 64;

	int i = 2, calcu = 0;
	int loca; 
	char buf[1024] = { 0 };
	GETBLK(buf, phsc[0]);
	loca = GETSTARTNUM(buf,phsc[1]);

	char b[1024] = { 0 };
	char num[5] = { 0 };
	GETBLK(b, loca);
	calcu = i = 0;
	while (b[i] != '|') 
	{ 
		num[calcu++] = b[i++]; 
	}
	++i;
	num[calcu] = '\0';
	int size = atoi(num);
	int calcu_a = 0, calcu_b = 0;
	char filen[10] = { 0 };
	while (calcu_a++ < size)
	{
		calcu_b = 0;
		memset(filen, 0, sizeof(filen));
		while (b[i] != ',') 
		{ 
			filen[calcu_b++] = b[i++]; 
		}
		++i;
		if (strcmp(filename, filen) == 0)
		{
			int m = 0;
			calcu_b = 0;
			while (b[i] != '|') 
			{
				num[calcu_b++] = b[i++]; 
			}
			num[calcu_b] = '\0';
			ino = atoi(num);

			phsc[0] = 4 + ino / 16;
			phsc[1] = ino % 16 * 64;

			i = 2, size = 0;
			GETBLK(buf, phsc[0]);
			//是普通文件的索引节点
			if (buf[phsc[1]] == '1')
			{
				while (buf[phsc[1] + i++] != '|');
				size = buf[phsc[1] + i] - '0'; 
				i += 2;

				calcu_a = 0, calcu_b = 0;
				memset(num, 0, sizeof(num));
				while (calcu_a++ < size)
				{
					calcu_b = 0;
					while (buf[phsc[1] + i] != '|') 
					{ 
						num[calcu_b++] = buf[phsc[1] + i++]; 
					}
					++i;
					num[calcu_b] = '\0';
					datablk[m++] = atoi(num);
				}
			}
			//是目录文件的索引节点
			else
			{
				while (buf[phsc[1] + i++] != '|');
				memset(num, 0, sizeof(num));
				calcu = 0;
				while (buf[phsc[1] + i] != '|') 
				{
					num[calcu++] = buf[phsc[1] + i++];
				}
				datablk[m++] = atoi(num);
			}
			for (; m < sizeof(datablk); m++)
				datablk[m] = -1;
			
			/*for(int h = 0; h < 8; h++)
				printf("%d,",datablk[h]);
			printf("\n");*/
			return ino;
		}
		while (b[i++] != '|');
	}
	for (i = 0; i < sizeof(datablk); i++)
		datablk[i] = -1;
	printf("		This file does not exists.\n");
	return -1;
}

//创建新文件的inode 
int CREATEININO(int ino, char filename[], int ino_file)    
{
	int phsc[2] = { 0 };
	int i = 0;
	char buf[1024] = { 0 };

	phsc[0] = 4 + ino / 16;
	phsc[1] = ino % 16 * 64;

	i = 2;
	int calcu = 0;
	int loca; 
	GETBLK(buf, phsc[0]);
	loca = GETSTARTNUM(buf, phsc[1]);

	//获取目录文件数据块并写入新文件信息 
	//判断是否有重名文件 
	char b[1024] = { 0 };
	char use1[1024] = { 0 };
	char num[5] = { 0 }; 
	GETBLK(b, loca);
	calcu = i = 0;
	while (b[i] != '|') { num[calcu++] = b[i++]; }++i;
	strncpy(use1, b + i - 1 ,sizeof(use1));
	num[calcu] = '\0';
	int size = atoi(num);
	int calcu_a = 0, calcu_b = 0;
	char filen[10] = { 0 };
	while (calcu_a++ < size)
	{
		calcu_b = 0;
		memset(filen, 0, sizeof(filen));
		while (b[i] != ',') 
		{ 
			filen[calcu_b++] = b[i++]; 
		}
		++i;
		if (strcmp(filename, filen) == 0)
		{
			printf("		This file is already exists!\n");
			printf("		You can open this file to rewrite it.\n");
			return -1;
		}
		while (b[i++] != '|');
	}
	sprintf(num, "%d\0", ++size);
	strncpy(b, num, sizeof(b));
	strcat(b, use1);
	strncat(b,filename,sizeof(b));
	strcat(b, ",");
	sprintf(num, "%d\0", ino_file);
	strcat(b, num);
	strcat(b, "|");
	PUTBLK(b, loca);

	//计算ino_file对应的物理地址
	phsc[0] = 4 + ino_file / 16;
	phsc[1] = ino_file % 16 * 64;

	//置此索引节点位图为'1' 
	GETBLK(buf, 3);
	buf[ino_file] = '1';
	PUTBLK(buf, 3);

	i = 2, size = 0;
	GETBLK(buf, phsc[0]);
	//是普通文件的索引节点
	if (buf[phsc[1]] == '1')
	{
		while (buf[phsc[1] + i++] != '|');
		size = buf[phsc[1] + i] - '0'; 
		i += 2; 
		//置数据块位图相应位为'1' 
		int calcu_a = 0, calcu_b = 0;
		char b[1024] = { 0 };
		char num[5] = { 0 };
		GETBLK(b, 2);
		while (calcu_a++ < size)
		{
			calcu_b = 0;
			while (buf[phsc[1] + i] != '|') 
			{ 
				num[calcu_b++] = buf[phsc[1] + i++]; 
			}
			++i;
			num[calcu_b] = '\0';
			b[atoi(num)] = '1';
		}
		PUTBLK(b, 2);
	}
	//是目录文件的索引节点
	else
	{
		while (buf[phsc[1] + i++] != '|');
		char num[5] = { 0 };
		int calcu = 0;
		while (buf[phsc[1] + i] != '|') 
		{ 
			num[calcu++] = buf[phsc[1] + i++]; 
		}
		num[calcu] = '\0';

		//置目录文件的数据块位图为'1'
		char b[1024] = { 0 };
		GETBLK(b, 2);
		b[atoi(num)] = '1';
		PUTBLK(b, 2);
	}
	return 0;
}

//展示路径
int SHOWPOSITION()      
{
	int i = 0, size = 0;
	int phsc[2] = { 0 };
	char buf[1024] = { 0 };
	printf("			");
	for (int m = 0; m < sizeof(path) && path[m] != -1; m++)
	{
		//计算ino对应的物理地址
		phsc[0] = 4 + path[m] / 16;
		phsc[1] = path[m] % 16 * 64;

		i = 2, size = 0;
		GETBLK(buf, phsc[0]);
		if (buf[phsc[1]] == '1')//是普通文件的索引节点
		{
			while (buf[phsc[1] + i] != '|')
			{
				printf("%c", buf[phsc[1] + i]);
				++i;
			}
		}
		else//是目录文件的索引节点
		{
			while (buf[phsc[1] + i] != '|')
			{
				printf("%c", buf[phsc[1] + i]);
				++i;
			}
			printf("/");
		}
	}
	printf("\n");
	return 0;
}
//操作文件 
void EDITFILE(char filename[])    
{
	int i = 0,n = 0;
	int ino,ino_file;
	bool flag = true;
	int DataBLK[8] = { -1,-1,-1,-1,-1,-1,-1,-1 };
	
	for (; i < sizeof(path) && path[i] != -1; ++i);
	ino = path[i - 1];
	ino_file = GETDATABLK(ino, filename, DataBLK);
	path[i] = ino_file;
	if (ino_file == -1) 
		return;

	while (flag)
	{
		ino_file = GETDATABLK(ino, filename, DataBLK);
		path[i] = ino_file;
		printf("		======================\n");
		printf("		The directory now is:\n");
		SHOWPOSITION();
		printf("		Edit this file:\n");
		printf("		1.write the file\n");
		printf("		2.read the file\n");
		printf("		3.close the file\n");
		printf("		======================\n");
		printf("		");
		scanf("%d", &n);
		getchar();
		
		switch (n)
		{
		case 1:
		{
			int x = 0;
			char file[1024] = { 0 };
			int datablk[1] = { 0 };
			int inode[2] = { 0 };
			char buf[1024] = { 0 };
			char use1[1024] = { 0 };
			char num[5] = { 0 };
			
			DELETEININO(ino, filename);
			printf("		What do you want to write to this file:\n");
			printf("		");
			scanf("%s",&file);
			TAKEDNOD(1, datablk);
			PUTBLK(file, datablk[0]);	//写入相应数据块 
			TAKEINOD(inode);
			
			GETBLK(buf, inode[0]);
			strcpy(use1,"1|");
			strcat(use1, filename);
			strcat(use1, "|1|");
			sprintf(num, "%d|\0", datablk[0]);
			strcat(use1, num);
			strcat(use1, "time|");
			
			while (use1[x]) 
			{ 
				buf[inode[1] + x] = use1[x]; ++x; 
			}
			buf[inode[1] + x] = 0;
			PUTBLK(buf, inode[0]);
			CREATEININO(ino, filename, (inode[0]-4)*5+inode[1]/64);
			break;
		}
		case 2:
		{
			int k = 0;
			char buf[1024] = { 0 };
			while (k < sizeof(DataBLK) && DataBLK[k] != -1)
			{
				GETBLK(buf, DataBLK[k]);
				printf("		%s", buf);
				k++;
			}
			printf("\n");
			break;
		}
		case 3:
			CLOSEFILE();
			flag = false;
			break;
		default:
			printf("		Input error!\n");
			break;
		}
	}
}

//ls指令
int LIST_DIRECTORY()
{
	int ino = 0, i = 0;
	for (; i < sizeof(path) && path[i] != -1; ++i);
	ino = path[i - 1];

	//计算ino物理地址 
	int phsc[2] = { 0 };
	phsc[0] = 4 + ino / 16;
	phsc[1] = ino % 16 * 64;

	i = 2;
	int calcu = 0, loca = 0;
	char buf[1024] = { 0 };
	char b[1024] = { 0 };
	char num[5] = { 0 };
	GETBLK(buf, phsc[0]);
	loca = GETSTARTNUM(buf, phsc[1]);
	//取出当前目录的data block 
	GETBLK(b, loca);
	calcu = i = 0;
	while (b[i] != '|') 
	{ 
		num[calcu++] = b[i++]; 
	}
	++i;
	num[calcu] = '\0';
	int size = atoi(num);
	if (!size)
	{
		printf("		 No file in The directory !\n");
		return 0;
	}
	int calcu_a = 0, calcu_b = 0;
	char filen[10] = { 0 };
	SETCOLOR(3,0);
	while (calcu_a++ < size)
	{
		calcu_b = 0;
		memset(filen, 0, sizeof(filen));
		while (b[i] != ',') 
		{ 
			filen[calcu_b++] = b[i++]; 
		}
		++i;
		printf("			%s\n", filen);
		while (b[i++] != '|');
	}
	SETCOLOR(10,0);
	return 0;
}

//cd指令 
int CHANGE_DIRECTORY()
{
	int ino = 0, ino_file = 0;
	int i = 0;
	for (; i < sizeof(path) && path[i] != -1; ++i);
	ino = path[i - 1];

	char filename[10] = { 0 };
	int datapbloc[1] = { -1 };
	printf("		Please input the directory file you want to enter:\n");
	printf("		");
	scanf("%s",&filename);
	if (!strcmp(filename, "back"))
	{
		path[i - 1] = -1;
		return 0;
	}
	ino_file = GETDATABLK(ino, filename, datapbloc);
	path[i] = ino_file;
	return 0;
}

//创建目录文件 
int CREAT_DIRFILE()
{
	char filename[10] = { 0 };
	printf("		Please input the directory file name:\n");
	printf("		");
	scanf("%s",&filename);

	char buf[1024] = { 0 };
	char use1[1024] = { 0 };
	char b[1024] = { 0 };
	int inode[2] = { 0 };
	int datablk[1] = { 0 };
	int data_no = 1, ino = 0;
	int i = 0;;
	char num[5] = { 0 };
	TAKEINOD(inode);
	GETBLK(buf, inode[0]);

	strcpy(use1,"0|");
	strcat(use1, filename);
	strcat(use1, "|");
	TAKEDNOD(data_no, datablk);
	GETBLK(b, datablk[0]);
	strcpy(b, "0|");
	PUTBLK(b, datablk[0]);
	sprintf(num, "%d\0", datablk[0]);
	strcat(use1, num);
	strcat(use1, "|");
	strcat(use1, "time|");
	//printf("%s\n",use1);
	int x = 0;
	while (use1[x]) { buf[inode[1] + x] = use1[x]; ++x; }
	buf[inode[1] + x] = 0;
	PUTBLK(buf, inode[0]);

	for (; i < sizeof(path) && path[i] != -1; ++i);
	ino = path[i - 1];
	CREATEININO(ino, filename, (inode[0] - 4) * 16 + inode[1] / 64);
	return 0;
}

//创建普通文件 
int CREAT_NORFILE()
{
	char filename[10] = { 0 };
	printf("		Please input the normal file name:\n");
	printf("		");
	scanf("%s",&filename);

	char buf[1024] = { 0 };
	char use1[1024] = { 0 };
	int inode[2] = { 0 };
	int ino = 0;
	int i = 0;
	TAKEINOD(inode);
	GETBLK(buf, inode[0]);

	strcpy(use1, "1|");
	strcat(use1, filename);
	strcat(use1, "|0|");
	strcat(use1, "time|");
	//printf("%s\n",use1);
	int x = 0;
	while (use1[x]) { buf[inode[1] + x] = use1[x]; ++x; }
	buf[inode[1] + x] = 0;
	PUTBLK(buf, inode[0]);

	for (; i < sizeof(path) && path[i] != -1; ++i);
	ino = path[i - 1];
	CREATEININO(ino, filename, (inode[0] - 4) * 16 + inode[1] / 64);
	return 0;
}

//删除文件 
int DELETE_FILE()
{
	char filename[10] = { 0 };
	printf("		Please input the file name:\n");
	printf("		");
	scanf("%s",&filename);
	
	int ino = 0;
	int i = 0;
	for (; i < sizeof(path) && path[i] != -1; ++i);
	ino = path[i - 1];
	DELETEININO(ino, filename);
	return 0;
}

//按照指定路径查找文件 
int FIND_FILE()
{
	printf("		eg.Root/f1/filex\n");
	printf("		Please input the path:\n");
	printf("		");
	
	int i = 0;
	char c = 0;
	char name[10] = { 0 };
	int ino_file = 0;
	int datablk[8] = { -1,-1,-1,-1,-1,-1,-1,-1 };
	i = 0;
	path[0] = 0;
	path[1] = path[2] = path[3] = path[4] = path[5] = path[6] = path[7] = path[8] = path[9] = -1;
	ino_file = 0;
	while (scanf("%c", &c) && c != '/');
	while (scanf("%c", &c) && c!='\n')
	{
		if (c == '/')
		{
			ino_file = GETDATABLK(ino_file, name, datablk);
			for (i = 0; i < sizeof(path) && path[i] != -1; ++i);
			path[i] = ino_file;
			memset(name, 0, sizeof(name));
			i = 0;
			continue;
		}
		name[i++] = c;
	}
	EDITFILE(name);
	return 0;
}

//从外部读入指定文件 
int READ_FILE()
{
	FILE *fin;
	int ByteNumOfFile = 0;
	char byt = 0;
	char file[8 * 1024] = { 0 };
	char path[100] = { 0 };
	char filename[10] = { 0 };
	printf("		eg.F:/hello.txt\n");
	printf("		Please input the outside file's path:\n");
	printf("		");
	scanf("%s",&path);
	fin=fopen(path,"r");
	if (!fin)
	{
		printf("		This file doesn't exist!\n");
		return 0;
	}
	while ((byt = fgetc(fin)) != EOF)
		file[ByteNumOfFile++] = byt;
	if (!ByteNumOfFile) 
	{ 
		printf("		It's a empty file!\n");
		fclose(fin); 
		return 0; 
	}
	if (ByteNumOfFile > 1024 * 8) 
	{ 
		printf("		Too large for our system !\n"); 
		fclose(fin); 
		return 0; 
	}
	
	printf("		please input the name of the file :\n");
	printf("		");
	scanf("%s",&filename);

	char buf[1024] = { 0 };
	char use1[1024] = { 0 };
	int inode[2] = { 0 };
	int datablk[8] = { -1,-1,-1,-1,-1,-1,-1,-1 };
	int data_no = (ByteNumOfFile - 1) / 1024 + 1;
	char num[5] = { 0 };
	int ino = 0;
	int i = 0, x = 0;
	
	//更新索引 
	TAKEINOD(inode);
	GETBLK(buf, inode[0]);
	strcpy(use1, "1|");
	strcat(use1, filename);
	sprintf(num, "%d\0", data_no);
	strcat(use1, "|");
	strcat(use1, num);
	strcat(use1, "|");
	TAKEDNOD(data_no, datablk);
	//写进相应数据块 
	for (int i = 0; i < data_no; i++)
	{
		char b[1024] = { 0 };
		GETBLK(b, datablk[i]);
		for (int j = i * 1024; j < sizeof(file) && j < (i + 1) * 1024; j++)
			b[j - i * 1024] = file[j];
		PUTBLK(b, datablk[i]);
		sprintf(num, "%d\0", datablk[i]);
		strcat(use1, num);
		strcat(use1, "|");
	}
	strcat(use1, "time|");
	
	while (use1[x]) 
	{ 
		buf[inode[1] + x] = use1[x]; 
		++x; 
	}
	buf[inode[1] + x] = 0;
	PUTBLK(buf, inode[0]);
	for (; i < sizeof(path) && path[i] != -1; ++i);
	ino = path[i - 1];
	CREATEININO(ino, filename, (inode[0] - 4) * 16 + inode[1] / 64);
	fclose(fin);
	return 0;
}

//主函数
int main()
{
	system("color 0A");
	int cho = 0; 
	bool flag5 = true;
	int flag4 = 0 ;
	char buf[1024] = { 0 };

	//创建文件系统
	f_sy=fopen("FILESYSTEM", "w+");
	if (!f_sy)
	{
		printf("		Open the file system failed!");
		system("pause");
		exit(1);
	}
	
	//初始化文件系统
	INITFILESYSTEM(f_sy);
	
	while(flag5)
    {
    	
        system("CLS");
        SETCOLOR(3,0);
        printf("		THE FILESYSTEM OF XXJ AND XRJ\n");
        SETCOLOR(10,0);
        printf("	******************************************************\n");
        printf("		|You are in the directory:  \n");
        SHOWPOSITION();
        printf("	******************************************************\n");
        printf("		|Please choice what you want to do:\n");
        printf("		|0. List all files in this directory\n");
        printf("		|1. Change directory\n");
        printf("		|2. Creat a new directory file\n");
        printf("		|3. Creat a new normal file\n");
        printf("		|4. Delete an existing file\n");
        printf("		|5. Find an existing file\n");
        printf("		|6. Read a file from outside\n");
        printf("	******************************************************\n");
        printf("		");
        scanf("%d", &cho);

        switch(cho){
        case 0:
            LIST_DIRECTORY();
            break;
        case 1:
            CHANGE_DIRECTORY();
            break;
        case 2:
            CREAT_DIRFILE();
            break;
        case 3:
            CREAT_NORFILE();
            break;
        case 4:
            DELETE_FILE();
            break;
        case 5:
            FIND_FILE();
            break;
        case 6:
            READ_FILE();
            break;
        default:
			printf("		Input error!\n");
			break;
        }
        printf("		Quit the system or not?(0.continue  1.quit)\n");
        printf("		");
        scanf("%d",&flag4);
        switch(flag4){
        case 0:
            flag5 = true ;
            break;
        case 1:
            flag5 = false ;
            break;
        default:
			printf("		Input error!\n");
			break;
        }
    }
    
	printf("	******************************************************\n");
    printf("		The file system is closed! \n");
    fclose(f_sy);
    return 0;
}
