#include<stdio.h>
#include<iostream>
#include<Windows.h>

//使用匿名管道来实现通信
int main()
{
	//创建匿名管道
	HANDLE output[2];	//管道的两端

	//管道的属性
	SECURITY_ATTRIBUTES la;
	la.nLength = sizeof(la);
	la.bInheritHandle = true;
	la.lpSecurityDescriptor = 0;

	bool isCreate = CreatePipe(&output[0], &output[1], &la, 0);
	if (isCreate == false)
	{
		MessageBox(0, "create cgi pipe error!", 0, 0);
		return 1;
	}

	//创建一个子进程
	char cmd[] = "ping www.baidu.com";
	//子进程的启动属性
	STARTUPINFO si = { 0 };
	si.cb = sizeof(si);
	si.hStdOutput = output[1];
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;

	PROCESS_INFORMATION pi = { 0 };

	bool isTrue = CreateProcess(NULL, cmd, 0, 0, TRUE, 0, 0, 0, &si, &pi);

	if (isTrue == false)
	{
		std::cout << "子进程创建失败！\n";
	}

	char buff[1024];
	DWORD size;
	while (1)
	{
		//std::cout << "请输入：";
		//gets_s(buff, sizeof(buff));	//向数组里读取内容

		//WriteFile(output[1], buff, sizeof(buff) + 1, &size, NULL); 
		//std::cout << "已经写入了" << size << "字节\n";

		ReadFile(output[0], buff, sizeof(buff), &size, NULL);
		buff[size] = '\0';
		std::cout << "已经读取了" << size << "字节：[" << buff << "]\n";
	}
	return 0;
}